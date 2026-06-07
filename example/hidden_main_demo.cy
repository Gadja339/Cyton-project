# Демонстрация hidden main — top-level код является точкой входа,
# fun main() не нужен.

fun square(int64 x) -> int64:
    return x * x;

fun greet(string name) -> unit:
    print("Привет, " + name + "!");

fun factorial(int64 n) -> int64:
    if n <= 1:
        return 1;
    return n * factorial(n - 1);

struct Point:
    int64 x
    int64 y
;

fun distance_sq(Point p) -> int64:
    return p.x * p.x + p.y * p.y;

# ── Точка входа — top-level код ───────────────────────────────────────────────

print("=== hidden main demo ===")

greet("Cyton")

print("square(7):")
print(square(7))

print("factorial(5):")
print(factorial(5))

let p = Point { x: 3, y: 4 }
print("distance_sq(3,4):")
print(distance_sq(p))

print("countdown:")
var i = 5
while i >= 1:
    print(i)
    i = i - 1;

print("done")
