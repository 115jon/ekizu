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
template <typename T = boost::blank>
using Result = boost::outcome_v2::result<T>;
// using Result = boost::outcome_v2::result<T, boost::system::error_code,
// 										 boost::outcome_v2::policy::terminate>;
namespace outcome = boost::outcome_v2;

}  // namespace ekizu

#endif	// EKIZU_RESULT_HPP
