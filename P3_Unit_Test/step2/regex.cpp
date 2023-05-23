#include<iostream>
#include<regex>

using namespace std;

int main(){
    regex re{"(w) (\\w+) (\\d+)\n"};
    cout<<regex_match("w hello 123\n", re)<<endl;
}