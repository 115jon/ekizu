#ifndef EKIZU_FUNCTION_VIEW_HPP
#define EKIZU_FUNCTION_VIEW_HPP

#include <type_traits>

namespace ekizu {
namespace detail {
template <typename T>
struct FunctionTraits : FunctionTraits<decltype(&T::operator())> {};

template <typename Class, typename Ret, typename... Params>
struct FunctionTraits<Ret (Class::*)(Params...) const> {
	using f_type = Ret(Params...);
};

template <typename Class, typename Ret, typename... Params>
struct FunctionTraits<Ret (Class::*)(Params...)> {
	using f_type = Ret(Params...);
};

template <typename Ret, typename... Params>
struct FunctionTraits<Ret (*)(Params...)> {
	using f_type = Ret(Params...);
};
}  // namespace detail

template <typename Fn>
struct FunctionView;

/**
 * @brief A lightweight, non-owning view of a callable.
 *
 * @tparam Ret The return type of the callable.
 * @tparam Params The parameter types of the callable.
 */
template <typename Ret, typename... Params>
struct FunctionView<Ret(Params...)> {
	FunctionView() = default;
	FunctionView(std::nullptr_t) {}	 // NOLINT

	template <
		typename Callable,
		typename std::enable_if_t<std::is_invocable_v<Callable, Params...> &&
								  !std::is_same_v<std::decay_t<Callable>,
												  FunctionView> > * = nullptr>
	FunctionView(Callable &&callable)  // NOLINT
		: m_closure{reinterpret_cast<void *>(&callable)},
		  m_proxy{[](void *closure, Params... params) {
			  return (*reinterpret_cast<std::remove_reference_t<Callable> *>(
				  closure))(params...);
		  }} {}

	explicit operator bool() const { return m_proxy != nullptr; }

	Ret operator()(Params... params) const {
		return m_proxy(m_closure, params...);
	}

   private:
	void *m_closure{};
	Ret (*m_proxy)(void *callable, Params... params){};
};

template <typename Fn>
FunctionView(Fn) -> FunctionView<typename detail::FunctionTraits<Fn>::f_type>;
}  // namespace ekizu

#endif	// EKIZU_FUNCTION_VIEW_HPP
