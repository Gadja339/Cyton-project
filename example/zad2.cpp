#include <iostream>
#include <vector>
#include <memory>

class Shape {
public:
    virtual double area() const = 0;
    virtual ~Shape() = default;
};

class Circle : public Shape {
    double r;

public:
    Circle(double r_) : r(r_) {}

    double area() const override {
        return 3.14 * r * r;
    }
};

class Rectangle : public Shape {
    double w, h;

public:
    Rectangle(double w_, double h_) : w(w_), h(h_) {}

    double area() const override {
        return w * h;
    }
};

std::vector<std::unique_ptr<Shape>>
createShapes(const std::vector<double>& params) {
    std::vector<std::unique_ptr<Shape>> shapes;

    if (params.size() == 1) {
        shapes.push_back(std::unique_ptr<Shape>(new Circle(params[0])));
    }

    if (params.size() == 2) {
        shapes.push_back(std::unique_ptr<Shape>(new Rectangle(params[0], params[1])));
    }

    return shapes;
}

int main() {
    auto circle = createShapes({5});
    auto rectangle = createShapes({3, 4});
    auto wrong = createShapes({1, 2, 3});

    for (const auto& shape : circle) {
        std::cout << "Circle: " << shape->area() << '\n';
    }

    for (const auto& shape : rectangle) {
        std::cout << "Rectangle: " << shape->area() << '\n';
    }

    return 0;
}