#include "reactive.h"
#include <iostream>
#include <string>

using namespace std;
using namespace reactive;

int copy_counter = 0;
int move_counter = 0;

int main()
{
	{
		auto task = range(20)
			|= filter([](int i) { return (i % 2) == 0; })
			|= accumulate(0)
			|= observe([](int i) { cout << i << "\n"; });
		task.start();

		cout << "\n";
		auto task2 = repeater(20, 2)
			|= filter([](int i) { return (i % 2) == 0; })
			|= accumulate(0)
			|= single_cast<int>();
		auto observer = filter([](int i) { return (i % 8) == 0; }) |= observe([](int i) { cout << i << "\n"; });
		task2.subscribe(observer);
		task2.start();
	}
	{
		cout << "\n";
		auto observable2 = range(20) |= single_cast<int>();
		auto observable = repeater(20, 2)
			|= filter([](int i) { return (i % 2) == 0; })
			|= accumulate(0)
			|= merge(observable2)
			|= single_cast<int>();

		auto my_filter = take(12) |= filter([](int i) { return (i % 4) == 0; }) |= single_cast<int>();
		
		observable_ref<int> o_ref = observable.get_observable_ref<int>();
		if(move_counter)
		{
			observable.subscribe(my_filter);
			o_ref = my_filter.get_observable_ref<int>();
		}

		auto observer = transform([](int i) -> float { return static_cast<float>(i) + 0.5f; })
			|= group(8)
			|= last()
			|= split()
			|= observe([](float i) { cout << i << "\n"; });
		o_ref.subscribe(observer);

		observable.start();
		observable2.start();
		observable.unsubscribe();
		observable2.unsubscribe();
	}
	/*
	{
		struct test_data
		{
			int i;
			std::string str;

			test_data() : i(1) {}
			test_data(int in_i) : i(in_i) {}
			test_data(const test_data& in) : i(in.i), str(in.str) { copy_counter++; }
			test_data& operator= (const test_data& d) { copy_counter++; i = d.i; str = d.str; return *this; }
			test_data(test_data&& in) : i(in.i), str(std::move(in.str)) { move_counter++; }
			test_data& operator= (test_data&& d) { move_counter++; i = d.i; str = std::move(d.str); return *this; }
		};

		old_repeater<test_data> observable(20, {});
		old_repeater<test_data> observable2(20, {});
		auto observer_part = merge(observable2)
			|= accumulate(test_data(), [](const test_data& a, const test_data& b) -> test_data { return test_data(a.i + b.i); })
			|= take(12)
			|= group(8)
			|= last()
			|= split()
			|= observe([](test_data i) { cout << i.i << "\n"; return status::open; });
		auto observer = filter([](const test_data& i) { return i.i == 1; }) 
			|= std::move(observer_part);

		observable.subscribe(observer);
		observable.start();
		observable2.start();
		observable.unsubscribe();
		observable2.unsubscribe();

		cout << "Copy: " << copy_counter << "\nMove: " << move_counter << "\n";
	}
	*/
}

/*
Observer:
	ends with observe
	API: next, complete

Observable:
	starts with a generator
	API: subscribe

Task: 
	starts with a generator 
	ends with observe
	API: start, stop, is_started

Block:
	|=	
*/