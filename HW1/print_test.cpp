#include <iostream>
#include <sstream>
using namespace std;

class C
{
	void test()
	{
		int a = 3;
	}
	void pt()
	{}
};

int main()
{
	/*string s = "asdf asaaadf", a = "", b = "", c = "";
	stringstream ss(s);
	ss >> a >> b >> c;
	cout << a << " " << b << " " << c << "\n";
	if(c == "") cout << "!@#\n";*/
	string a = "";
	cin >> a;
	cout << a.length() << "\n";
	
	return 0;
}
