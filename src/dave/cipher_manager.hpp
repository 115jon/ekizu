#ifndef DAVE_CIPHER_MANAGER_HPP
#define DAVE_CIPHER_MANAGER_HPP

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <optional>

#include "aead_cipher.hpp"
#include "common.hpp"
#include "key_ratchet.hpp"

namespace ekizu::dave::detail {

/**
 * @brief Compute wrapped generation handling wraparound
 */
inline key_generation compute_wrapped_generation(key_generation oldest,
												 key_generation generation) {
	auto remainder = oldest % GENERATION_WRAP;
	auto factor = oldest / GENERATION_WRAP + (generation < remainder ? 1 : 0);
	return factor * GENERATION_WRAP + generation;
}

/**
 * @brief Compute full nonce from generation and truncated nonce
 */
inline big_nonce compute_wrapped_big_nonce(key_generation generation,
										   truncated_sync_nonce nonce) {
	auto masked_nonce = nonce & ((1 << RATCHET_GENERATION_SHIFT_BITS) - 1);
	return static_cast<big_nonce>(generation) << RATCHET_GENERATION_SHIFT_BITS |
		   masked_nonce;
}

/**
 * @brief AEAD Cipher Manager with nonce tracking
 *
 * Manages cipher instances for different key generations and tracks
 * processed nonces to prevent replay attacks.
 */
struct CipherManager {
	using TimePoint = std::chrono::steady_clock::time_point;
	using Duration = std::chrono::seconds;

	CipherManager(std::unique_ptr<KeyRatchetInterface> ratchet)
		: m_ratchet(std::move(ratchet)),
		  m_creation_time(std::chrono::steady_clock::now()),
		  m_expiry_time(TimePoint::max()) {}

	/**
	 * @brief Check if a nonce can be processed (not already seen)
	 */
	[[nodiscard]] bool can_process_nonce(key_generation generation,
										 truncated_sync_nonce nonce) const {
		if (!m_newest_processed_nonce) { return true; }

		auto wrapped = compute_wrapped_big_nonce(generation, nonce);
		if (wrapped > *m_newest_processed_nonce) { return true; }

		// Check if in missing nonces list
		for (auto it = m_missing_nonces.rbegin(); it != m_missing_nonces.rend();
			 ++it) {
			if (*it == wrapped) { return true; }
		}
		return false;
	}

	/**
	 * @brief Report successful decryption for nonce tracking
	 */
	void report_cipher_success(key_generation generation,
							   truncated_sync_nonce nonce) {
		auto wrapped = compute_wrapped_big_nonce(generation, nonce);

		if (!m_newest_processed_nonce) {
			m_newest_processed_nonce = wrapped;
		} else if (wrapped > *m_newest_processed_nonce) {
			// Add missing nonces to queue
			auto oldest_missing =
				wrapped > MAX_MISSING_NONCES ? wrapped - MAX_MISSING_NONCES : 0;

			// Trim old missing nonces
			while (!m_missing_nonces.empty() &&
				   m_missing_nonces.front() < oldest_missing) {
				m_missing_nonces.pop_front();
			}

			// Add new missing nonces
			auto range_start =
				std::max(oldest_missing, *m_newest_processed_nonce + 1);
			for (auto i = range_start; i < wrapped; ++i) {
				m_missing_nonces.push_back(i);
			}

			m_newest_processed_nonce = wrapped;
		} else {
			// Remove from missing list if present
			for (auto it = m_missing_nonces.begin();
				 it != m_missing_nonces.end(); ++it) {
				if (*it == wrapped) {
					m_missing_nonces.erase(it);
					break;
				}
			}
		}

		// Update generation tracking
		if (generation > m_newest_generation) {
			m_newest_generation = generation;

			// Set expiry for old ciphers
			auto expiry = std::chrono::steady_clock::now() + CIPHER_EXPIRY;
			for (auto &[gen, cipher_entry] : m_ciphers) {
				if (gen < m_newest_generation &&
					cipher_entry.expiry == TimePoint::max()) {
					cipher_entry.expiry = expiry;
				}
			}
		}
	}

	/**
	 * @brief Get cipher for a specific generation
	 */
	AeadCipher *get_cipher(key_generation generation) {
		cleanup_expired_ciphers();

		if (generation < m_oldest_generation) { return nullptr; }
		if (generation > m_newest_generation + MAX_GENERATION_GAP) {
			return nullptr;
		}

		// Check ratchet lifetime
		auto lifetime = std::chrono::duration_cast<std::chrono::seconds>(
							std::chrono::steady_clock::now() - m_creation_time)
							.count();
		auto max_gen =
			MAX_FRAMES_PER_SECOND * lifetime >> RATCHET_GENERATION_SHIFT_BITS;
		if (generation > static_cast<key_generation>(max_gen)) {
			return nullptr;
		}

		auto it = m_ciphers.find(generation);
		if (it == m_ciphers.end()) {
			auto key = m_ratchet->get_key(generation);
			if (key.empty()) { return nullptr; }

			auto expiry = generation < m_newest_generation
							  ? std::chrono::steady_clock::now() + CIPHER_EXPIRY
							  : TimePoint::max();

			auto [inserted_it, _] = m_ciphers.emplace(
				generation,
				CipherEntry{std::make_shared<AeadCipher>(key), expiry});
			it = inserted_it;
		}

		return it->second.cipher.get();
	}

	/**
	 * @brief Compute wrapped generation relative to oldest
	 */
	[[nodiscard]] key_generation compute_wrapped_generation(
		key_generation generation) const {
		return detail::compute_wrapped_generation(
			m_oldest_generation, generation);
	}

	/**
	 * @brief Update expiry time
	 */
	void update_expiry(TimePoint expiry) {
		m_expiry_time = std::min(m_expiry_time, expiry);
	}

	/**
	 * @brief Check if manager is expired
	 */
	bool is_expired() const {
		return m_expiry_time < std::chrono::steady_clock::now();
	}

	/**
	 * @brief Get the key ratchet
	 */
	KeyRatchetInterface *ratchet() { return m_ratchet.get(); }

   private:
	struct CipherEntry {
		std::shared_ptr<AeadCipher> cipher;
		TimePoint expiry{TimePoint::max()};
	};

	void cleanup_expired_ciphers() {
		auto now = std::chrono::steady_clock::now();
		for (auto it = m_ciphers.begin(); it != m_ciphers.end();) {
			if (it->second.expiry < now) {
				it = m_ciphers.erase(it);
			} else {
				++it;
			}
		}

		// Delete old keys from ratchet
		while (m_oldest_generation < m_newest_generation &&
			   m_ciphers.find(m_oldest_generation) == m_ciphers.end()) {
			m_ratchet->delete_key(m_oldest_generation);
			++m_oldest_generation;
		}
	}

	std::unique_ptr<KeyRatchetInterface> m_ratchet;
	std::map<key_generation, CipherEntry> m_ciphers;
	std::deque<big_nonce> m_missing_nonces;
	std::optional<big_nonce> m_newest_processed_nonce;
	key_generation m_oldest_generation{0};
	key_generation m_newest_generation{0};
	TimePoint m_creation_time;
	TimePoint m_expiry_time;
};

}  // namespace ekizu::dave::detail

#endif	// DAVE_CIPHER_MANAGER_HPP
