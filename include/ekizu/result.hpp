#ifndef EKIZU_RESULT_HPP
#define EKIZU_RESULT_HPP

// NOTE: No exceptions should be the goal.
// #define BOOST_NO_EXCEPTIONS
#define BOOST_NO_IOSTREAM
#include <boost/blank.hpp>
#include <boost/outcome/result.hpp>
#include <boost/outcome/try.hpp>

#define EKIZU_TRY BOOST_OUTCOME_TRY
#define EKIZU_TRYV BOOST_OUTCOME_TRYV

namespace ekizu {
namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;
template <typename T = boost::blank>
using Result = outcome::result<T>;
// using Result = outcome::result<T, boost::system::error_code,
// 										 outcome::policy::terminate>;
}  // namespace ekizu

#endif	// EKIZU_RESULT_HPP
