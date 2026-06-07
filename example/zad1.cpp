#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

struct Employee {
    int id;
    std::string name;
    double salary;
};

bool lowSalary(const Employee& e) {
    return e.salary < 500;
}

bool compareEmployees(const Employee& a, const Employee& b) {
    if (a.salary != b.salary)
        return a.salary > b.salary;

    else return a.id < b.id;
}

int main() {
    std::vector<Employee> employees = {
        {1, "Kostya", 120},
        {2, "Vanya", 150},
        {3, "Zkdan", 900},
        {4, "Ya", 120},
        {5, "Dilnaz", 700}
    };

    employees.erase(std::remove_if(employees.begin(), employees.end(), [](const Employee& e){return e.salary < 500;} ), employees.end());

    std::sort(employees.begin(), employees.end(), [](const Employee& a, const Employee& b){return (a.salary != b.salary) ? (a.salary > b.salary) : (a.id < b.id);});

    int count = employees.size();

    if (count > 3) {
        count = 3;
    }

    for (int i = 0; i < count; ++i) {
        std::cout << employees[i].name << ": " << employees[i].salary << '\n';
    }

    return 0;
}