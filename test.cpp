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
		repeater<int> observable(20, 2);
		//range observable(20);
		range observable2(20);
		auto observer = filter([](int i) { return (i % 2) == 0; })
			|= accumulate(0)
			|= merge(observable2)
			|= take(12)
			|= transform([](int i) -> float { return static_cast<float>(i) + 0.5f; })
			|= group(8)
			|= last()
			|= split()
			|= simple_receiver([](float i) { cout << i << "\n"; });

		observable.subscribe(observer);
		observable.start();
		observable2.start();
		observable.unsubscribe();
		observable2.unsubscribe();
	}

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

		repeater<test_data> observable(20, {});
		repeater<test_data> observable2(20, {});
		auto observer = filter([](const test_data& i) { return i.i == 1; })
			|= merge(observable2)
			|= accumulate(test_data(), [](const test_data& a, const test_data& b) -> test_data { return test_data(a.i + b.i); })
			|= take(12)
			|= group(8)
			|= last()
			|= split()
			|= simple_receiver([](const test_data& i) { cout << i.i << "\n"; });
		observable.subscribe(observer);
		observable.start();

		cout << "Copy: " << copy_counter << "\nMove: " << move_counter << "\n";
	}
}