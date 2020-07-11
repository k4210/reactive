#pragma once
#include <functional>
#include <optional>
#include <memory>
#include <vector>
#include <assert.h>

namespace reactive
{
	enum class status
	{
		open,
		closed,
		error
	};
/*
	template <class T> struct observer
	{
		status next(T);
		void complete();
	};

	template <class T> struct observable
	{
		void subscribe(observer<T>&);
	};

	template <class T> struct lifter
	{
		|=
	};
*/

// OBSERVABLE
	struct range
	{
		int stop = 0;

		range(int in_stop)
			: stop(in_stop) {}

		std::function<status(int)> next;
		std::function<void()> complete;

		void unsubscribe()
		{
			next = nullptr;
			complete = nullptr;
			stop = 0;
		}
		void start()
		{
			assert(next);
			assert(complete);
			for (int it = 0; it < stop; ++it)
			{
				if (next(it) != status::open)
				{
					unsubscribe();
					return;
				}
			}

			complete();
		}
		template<typename type_inner_observer> void subscribe(type_inner_observer& observer)
		{
			assert(!next);
			assert(!complete);
			next = [&observer](int val) -> status { return observer.next(val); };
			complete = [&observer]() { observer.complete(); };
		}
	};

	template<typename T>
	struct repeater
	{
		int stop = 0;
		T val;

		repeater(int in_stop, T v) : stop(in_stop), val(std::move(v)) {}

		std::function<status(T)> next;
		std::function<void()> complete;

		void unsubscribe()
		{
			next = nullptr;
			complete = nullptr;
			stop = 0;
		}
		void start()
		{
			assert(next);
			assert(complete);
			for (int it = 0; it < stop; ++it)
			{
				if (next(val) != status::open)
				{
					unsubscribe();
					return;
				}
			}

			complete();
		}
		template<typename type_inner_observer> void subscribe(type_inner_observer& observer)
		{
			assert(!next);
			assert(!complete);
			next = [&observer](T val){ return observer.next(std::move(val)); };
			complete = [&observer]() { observer.complete(); };
		}
	};
//
	namespace details
	{
		template<typename F, typename Ret, typename A, typename... Rest> A helper_first(Ret(F::*)(A, Rest...));
		template<typename F, typename Ret, typename A, typename... Rest> A helper_first(Ret(F::*)(A, Rest...) const);
		template<typename F, typename Ret, typename A, typename B, typename... Rest> B helper_sec(Ret(F::*)(A, B, Rest...));
		template<typename F, typename Ret, typename A, typename B, typename... Rest> B helper_sec(Ret(F::*)(A, B, Rest...) const);
		template<typename F> struct lambda_first_argument 
		{
			typedef decltype(helper_first(&F::operator())) type;
		};
		template<typename F> struct lambda_second_argument
		{
			typedef decltype(helper_sec(&F::operator())) type;
		};
		template<typename F> struct observer_argument
		{
			typedef decltype(helper_first(&F::next)) type;
		};


		template <typename type_func> struct filter_imp
		{
			type_func func;

			template<typename type_inner_observer> struct composite_observer
			{
				type_inner_observer observer;
				void complete() { observer.complete(); }
				using T = typename observer_argument<type_inner_observer>::type;

				type_func func;

				status next(T arg)
				{
					return func(arg) ? observer.next(std::move(arg)) : status::open;
				}
			};

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)&
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), func};
			}

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)&&
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), std::move(func)};
			}
		};

		struct take_imp
		{
			unsigned int num = 1;

			template<typename type_inner_observer> struct composite_observer
			{
				type_inner_observer observer;
				void complete() { observer.complete(); }
				using T = typename observer_argument<type_inner_observer>::type;

				unsigned int num = 1;

				status next(T arg)
				{
					const status result = observer.next(std::move(arg));
					assert(num);
					if (--num == 0)
					{
						observer.complete();
						return status::closed;
					}
					return result;
				}
			};

			template<typename type_inner_observer> auto operator |=(type_inner_observer&& inner_observer)
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), num};
			}
		};

		template <typename type_func> struct transform_imp
		{
			type_func func;

			template<typename type_inner_observer> struct composite_observer
			{
				type_inner_observer observer;
				void complete() { observer.complete(); }

				using T = typename lambda_first_argument< type_func >::type;

				type_func func;

				status next(T arg)
				{
					return observer.next(func(arg));
				}
			};

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)&
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), func};
			}

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)&&
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), std::move(func)};
			}
		};

		struct last_imp
		{
			template<typename type_inner_observer> struct composite_observer
			{
				type_inner_observer observer;
				using T = typename observer_argument<type_inner_observer>::type;

				std::optional<T> last_val;

				status next(T arg)
				{
					last_val = arg;
					return status::open;
				}

				void complete()
				{
					if (last_val)
					{
						observer.next(std::move(*last_val));
						last_val.reset();
					}
					observer.complete();
				}
			};

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer)};
			}
		};

		template <typename type_func, typename T> struct accumulate_imp
		{
			type_func func;
			T initial_val;

			template<typename type_inner_observer> struct composite_observer
			{
				type_inner_observer observer;
				void complete() { observer.complete(); }

				type_func func;
				T val;

				status next(T arg)
				{
					val = func(val, arg);
					return observer.next(val);
				}
			};

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)&
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), func, initial_val};
			}

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)&&
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), std::move(func), std::move(initial_val)};
			}
		};

		template <typename type_observable> struct merge_imp
		{
			type_observable& observable;

			template<typename type_inner_observer> struct composite_observer_passed
			{
				std::unique_ptr<type_inner_observer> common_observer;
				using T = typename observer_argument<type_inner_observer>::type;
				status next(T arg)
				{
					assert(common_observer);
					return common_observer->next(std::move(arg));
				}

				composite_observer_passed() = default;
				composite_observer_passed(composite_observer_passed&&) = default;
				composite_observer_passed& operator=(composite_observer_passed&&) = default;
				composite_observer_passed(const composite_observer_passed&) = delete;
				composite_observer_passed& operator=(const composite_observer_passed&) = delete;

				void complete()
				{
					assert(common_observer);
					common_observer->complete();
				}
			};

			template<typename type_inner_observer> struct composite_observer_common
			{
				type_inner_observer observer;
				int observers_to_complete = 2;
				status recent_status = status::open;
				using T = typename observer_argument<type_inner_observer>::type;
				status next(T arg)
				{
					assert(observers_to_complete);
					if (status::open == recent_status)
					{
						recent_status = observer.next(std::move(arg));
					}
					return recent_status;
				}

				void complete()
				{
					assert(observers_to_complete);
					--observers_to_complete;
					if (!observers_to_complete)
					{
						observer.complete();
					}
				}
			};

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)
			{
				composite_observer_passed<composite_observer_common<type_inner_observer>> passed_observer;
				passed_observer.common_observer.reset(new composite_observer_common<type_inner_observer>{ std::move(inner_observer) });
				observable.subscribe(*passed_observer.common_observer);
				return passed_observer;
			}
		};

		struct group_imp
		{
			const unsigned int num;
			
			template<typename type_inner_observer> struct composite_observer
			{
				type_inner_observer observer;
				const unsigned int num;

				using T = typename observer_argument<type_inner_observer>::type::value_type;
				std::vector<T> v;

				composite_observer(type_inner_observer&& in_o, unsigned int in_num)
					: observer(std::move(in_o)), num(in_num)
				{
					v.reserve(num);
				}

				status next(T arg)
				{
					v.push_back(std::move(arg));
					if (v.size() == num)
					{
						const status result = observer.next(std::move(v));
						v.clear();
						v.reserve(num);
						return result;
					}
					return status::open;
				}

				void complete()
				{
					if (v.size())
					{
						observer.next(std::move(v));
						v.clear();
					}
					observer.complete();
				}
			};

			template<typename type_inner_observer> auto operator |=(type_inner_observer&& inner_observer)
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), num};
			}
		};

		struct split_imp
		{
			template<typename type_inner_observer> struct composite_observer
			{
				type_inner_observer observer;

				using T = typename observer_argument<type_inner_observer>::type;

				status next(std::vector<T> arg)
				{
					for(auto& val : arg)
					{
						const status result = observer.next(std::move(val));
						if (result != status::open)
							return result;
					}
					return status::open;
				}

				void complete()
				{
					observer.complete();
				}
			};

			template<typename type_inner_observer> auto operator |=(type_inner_observer&& inner_observer)
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer)};
			}
		};

		template <typename type_next> struct simple_receiver_imp
		{
			type_next next_func;

			using T = typename std::decay< typename details::lambda_first_argument<type_next>::type >::type;
			status next(T arg)
			{
				if constexpr (std::is_same_v<status, decltype(next_func(arg))>)
				{
					return next_func(std::forward<T>(arg));
				}
				else
				{
					next_func(std::forward<T>(arg));
					return status::open;
				}
			}
			void complete() {}

			simple_receiver_imp(simple_receiver_imp&&) = default;
			simple_receiver_imp(type_next&& in_next) : next_func(in_next) {}
		};

		template <typename type_next, typename type_complete> struct receiver_imp : public simple_receiver_imp<type_next>
		{
			type_complete complete_func;
			void complete()
			{
				complete_func();
			}

			receiver_imp(receiver_imp&&) = default;
			receiver_imp(type_next&& in_next, type_complete&& in_complete)
				: simple_receiver_imp(in_next), complete_func(in_complete) {}
		};
	}

// MODIFIERS
	auto take(unsigned int num)
	{
		return details::take_imp{ num };
	}

	auto last()
	{
		return details::last_imp{};
	}

	auto group(unsigned int num)
	{
		return details::group_imp{ num };
	}

	auto split()
	{
		return details::split_imp{};
	}
	
	template <typename type_func> auto filter(type_func&& func)
	{
		return details::filter_imp<type_func>{func};
	}

	template <typename type_func> auto transform(type_func&& func)
	{
		return details::transform_imp<type_func>{func};
	}

	template <typename type_func, typename T> auto accumulate(T inital_val, type_func&& func)
	{
		return details::accumulate_imp< type_func, T>{std::move(func), std::move(inital_val)};
	}
	
	template <typename T> auto accumulate(T inital_val)
	{
		auto sum = [](const T& a, const T& b) -> T { return a + b; };
		return details::accumulate_imp< decltype(sum), T>{std::move(sum), std::move(inital_val)};
	}

	template <typename type_observable> auto merge(type_observable& observable)
	{
		return details::merge_imp<type_observable>{observable};
	}

// OBSERVER

	template <typename type_next, typename type_complete> auto observe(type_next func_next, type_complete func_complete)
	{
		return details::receiver_imp<type_next, type_complete>(std::move(func_next), std::move(func_complete));
	}

	// void(T) 
	// void(T&&)
	// void(const T&)
	// status(T)
	// status(T&&) 
	// status(const T&)
	template <typename type_next> auto observe(type_next func_next)
	{
		return details::simple_receiver_imp(std::move(func_next));
	}
};
