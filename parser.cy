type Meters = int64

struct Point:
    int64 x
    int64 y
;



fun add(int64 a, int64 b) -> int64:
    return a + b
;

let x = 10
let y = 20
let z = add(x, y)

let p = Point { x: 10, y: 20 }

if z > 20:
    print("big")
else:
    print("small")
;

var i = 3

while i > 0:
    print(i)
    i = i - 1
;

print(Math::square(z))
print(p.x)