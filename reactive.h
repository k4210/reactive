#pragma once
#include <functional>
#include <optional>
#include <memory>
#include <vector>
#include <assert.h>

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

namespace reactive
{
	enum class status
	{
		open,
		closed,
		error
	};

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

		template<typename type_inner_observer> struct base_block
		{
			type_inner_observer observer;
			
			void complete() { observer.complete(); }
			template<typename type_final_observer> void subscribe(type_final_observer& final_observer)
			{
				observer.subscribe(final_observer);
			}
			void unsubscribe()
			{
				observer.unsubscribe();
			}

			base_block(type_inner_observer&& in_observer) : observer(std::move(in_observer)) {}
			base_block(base_block&& in_block) : observer(std::move(in_block.observer)) {}
		};

		template <typename type_func> struct filter_block
		{
			type_func func;

			template<typename type_inner_observer> struct composite_observer : public base_block<type_inner_observer>
			{
				using T = typename observer_argument<type_inner_observer>::type;
				using super = typename base_block<type_inner_observer>;

				type_func func;
				
				status next(T arg)
				{
					return func(arg) ? super::observer.next(std::move(arg)) : status::open;
				}

				composite_observer(type_inner_observer&& in_block, type_func&& in_func) : super(std::move(in_block)), func(in_func) {}
			};

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)&&
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), std::move(func)};
			}
		};

		struct take_imp
		{
			unsigned int num = 1;

			template<typename type_inner_observer> struct composite_observer : public base_block<type_inner_observer>
			{
				using T = typename observer_argument<type_inner_observer>::type;
				using super = typename base_block<type_inner_observer>;

				unsigned int num = 1;

				status next(T arg)
				{
					const status result = super::observer.next(std::move(arg));
					assert(num);
					if (--num == 0)
					{
						super::observer.complete();
						return status::closed;
					}
					return result;
				}

				composite_observer(type_inner_observer&& in_block, unsigned int in_num) : super(std::move(in_block)), num(in_num) {}
			};

			template<typename type_inner_observer> auto operator |=(type_inner_observer&& inner_observer)
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), num};
			}
		};

		template <typename type_func> struct transform_imp
		{
			type_func func;

			template<typename type_inner_observer> struct composite_observer : public base_block<type_inner_observer>
			{
				using T = typename lambda_first_argument<type_func>::type;
				using super = typename base_block<type_inner_observer>;

				type_func func;

				status next(T arg)
				{
					return super::observer.next(func(std::forward<T>(arg)));
				}

				composite_observer(type_inner_observer&& in_block, type_func&& in_func) : super(std::move(in_block)), func(std::move(in_func)) {}
			};

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)&&
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), std::move(func)};
			}
		};

		struct last_imp
		{
			template<typename type_inner_observer> struct composite_observer : public base_block<type_inner_observer>
			{
				using T = typename observer_argument<type_inner_observer>::type;
				using super = typename base_block<type_inner_observer>;

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
						super::observer.next(std::move(*last_val));
						last_val.reset();
					}
					super::observer.complete();
				}

				composite_observer(type_inner_observer&& in_block) : super(std::move(in_block)) {}
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

			template<typename type_inner_observer> struct composite_observer : public base_block<type_inner_observer>
			{
				using super = typename base_block<type_inner_observer>;

				type_func func;
				T val;

				status next(T arg)
				{
					val = func(val, arg);
					return super::observer.next(val);
				}

				composite_observer(type_inner_observer&& in_block, type_func&& in_func, T&& in_val) 
					: super(std::move(in_block)), func(std::move(in_func)), val(std::move(in_val)) {}
			};

			template<typename type_inner_observer>
			auto operator |=(type_inner_observer&& inner_observer)&&
			{
				return composite_observer<type_inner_observer>(std::move(inner_observer), std::move(func), std::move(initial_val));
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

				template<typename type_final_observer> void subscribe(type_final_observer& final_observer)
				{
					assert(common_observer);
					common_observer->subscribe(final_observer);
				}
				void unsubscribe()
				{
					assert(common_observer);
					common_observer->unsubscribe();
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

				template<typename type_final_observer> void subscribe(type_final_observer& final_observer)
				{
					observer.subscribe(final_observer);
				}
				void unsubscribe()
				{
					observer.unsubscribe();
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

			template<typename type_inner_observer> struct composite_observer : public base_block<type_inner_observer>
			{
				using T = typename observer_argument<type_inner_observer>::type::value_type;
				using super = typename base_block<type_inner_observer>;

				const unsigned int num;
				std::vector<T> v;

				composite_observer(type_inner_observer&& in_o, unsigned int in_num)
					: super(std::move(in_o)), num(in_num)
				{
					v.reserve(num);
				}

				status next(T arg)
				{
					v.push_back(std::move(arg));
					if (v.size() == num)
					{
						const status result = super::observer.next(std::move(v));
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
						super::observer.next(std::move(v));
						v.clear();
					}
					super::observer.complete();
				}
			};

			template<typename type_inner_observer> auto operator |=(type_inner_observer&& inner_observer)
			{
				return composite_observer<type_inner_observer>{std::move(inner_observer), num};
			}
		};

		struct split_imp
		{
			template<typename type_inner_observer> struct composite_observer : public base_block<type_inner_observer>
			{
				using T = typename observer_argument<type_inner_observer>::type;
				using super = typename base_block<type_inner_observer>;

				status next(std::vector<T> arg)
				{
					for (auto& val : arg)
					{
						const status result = super::observer.next(std::move(val));
						if (result != status::open)
							return result;
					}
					return status::open;
				}

				composite_observer(type_inner_observer&& in_block) : super(std::move(in_block)){}
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

		struct range_imp
		{
			unsigned int stop = 0;

			template<typename type_observable_block> struct imp
			{
				type_observable_block block;

				template<typename type_inner_observer> void subscribe(type_inner_observer& observer)
				{
					block.subscribe(observer);
				}
				void unsubscribe()
				{
					block.unsubscribe();
				}

				unsigned int stop = 0;
				void start()
				{
					for (int it = 0; it < static_cast<int>(stop); ++it)
					{
						if (block.next(it) != status::open)
						{
							return;
						}
					}

					block.complete();
				}
			};

			template<typename type_observable_block>
			auto operator |=(type_observable_block&& inner_observer)
			{
				return imp<type_observable_block>{ std::move(inner_observer), stop };
			}
		};

		template<typename T> struct repeater_imp
		{
			unsigned int num;
			T value;

			template<typename type_observable_block> struct imp
			{
				type_observable_block block;
				unsigned int num = 0;
				T value;

				template<typename type_inner_observer> void subscribe(type_inner_observer& observer)
				{
					block.subscribe(observer);
				}
				void unsubscribe()
				{
					block.unsubscribe();
				}

				void start()
				{
					for (unsigned int it = 0; it < num; ++it)
					{
						if (block.next(value) != status::open)
						{
							return;
						}
					}
					block.complete();
				}
			};

			template<typename type_observable_block>
			auto operator |=(type_observable_block&& inner_observer)&&
			{
				return imp<type_observable_block>{ std::move(inner_observer), num, std::move(value) };
			}
		};
	}

// OBSERVABLE
	auto range(unsigned int num)
	{
		return details::range_imp{ num };
	}

	template<typename T> auto repeater(unsigned int num, T value)
	{
		return details::repeater_imp<T>{ num, std::move(value) };
	}

// ???
	template<typename T> struct single_cast_imp
	{
		std::function<status(T)> func_next;
		std::function<void()> func_complete;

		status next(T arg)
		{
			assert(func_next);
			return func_next(std::move(arg));
		}

		void complete()
		{
			assert(func_complete);
			func_complete();
		}

		template<typename type_inner_observer> void subscribe(type_inner_observer& observer)
		{
			assert(!func_next);
			assert(!func_complete);
			func_next = [&observer](T val) { return observer.next(std::move(val)); };
			func_complete = [&observer]() { observer.complete(); };
		}

		void unsubscribe()
		{
			func_next = nullptr;
			func_complete = nullptr;
		}
	};

	template<typename T> auto single_cast()
	{
		return single_cast_imp<T>{};
	}
//BLOCKS
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
		return details::filter_block<type_func>{func};
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
