fun isPrime(int64 n) -> bool:
    if n < 2:
        return false;

    var i = 2

    while i * i <= n:
        if n % i == 0:
            return false;

        i = i + 1;

    return true;

fun classifyNumber(int64 n) -> unit:
    var bool done = false

    if done == false and n % 2 == 0 and n > 100:
        print("BigEven")
        done = true;

    if done == false and n % 2 == 0 and n >= 10 and n <= 100:
        print("MediumEven")
        done = true;

    if done == false and n % 2 == 0 and n < 10:
        print("SmallEven")
        done = true;

    if done == false and n % 2 != 0 and n >= 100 and n <= 999:
        print("OddThreeDigit")
        done = true;

    if done == false and n % 2 != 0 and n < 0:
        print("NegativeOdd")
        done = true;

    if done == false and n % 2 != 0 and isPrime(n):
        print("PrimeOdd")
        done = true;

    if done == false:
        print("Other");
;

fun main() -> int64:
    var arr = [1,2,3,7,17,0,111,8955554,100, 42, -32, 890, 13]
    var i = 0
    while i < 10:
        classifyNumber(arr[i])
        i = i + 1;

    return 0
;