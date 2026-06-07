


fun NumGenerer(string s) -> int64:
    if s == "2":
        return 2;

    if s == "3":
        return 3;

    if s == "4":
        return 4;

    if s == "5":
        return 5;

    return 0;

fun calculateAverage(array grades, int64 count) -> float64:
    if count == 0:
        return 0.0;

    var i = 0
    var sum = 0.0
    var countFloat = 0.0

    while i < count:
        if grades[i] == 2:
            sum = sum + 2.0;

        if grades[i] == 3:
            sum = sum + 3.0;

        if grades[i] == 4:
            sum = sum + 4.0;

        if grades[i] == 5:
            sum = sum + 5.0;

        countFloat = countFloat + 1.0
        i = i + 1;

    return sum / countFloat;

fun goodChet(array grades, int64 count) -> int64:
    var i = 0
    var result = 0

    while i < count:
        if grades[i] == 5:
            result = result + 1;

        i = i + 1;

    return result;

fun badschet(array grades, int64 count) -> int64:
    var i = 0
    var result = 0

    while i < count:
        if grades[i] == 2:
            result = result + 1;

        i = i + 1;

    return result;

fun main() -> int64:
    var grades = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]

    var count = 0
    var done = false
    var text = ""
    var grade = 0

    while done == false and count < 100:
        text = input()
        grade = NumGenerer(text)

        if grade == 0:
            done = true
        else:
            grades[count] = grade
            count = count + 1;
    ;

    var average = calculateAverage(grades, count)
    var excellent = goodChet(grades, count)
    var bad = badschet(grades, count)

    print(average)
    print(excellent)
    print(bad)

    if average > 4.5:
        print("Красный диплом")
    else:
        if bad > 0:
            print("Отчисление")
        else:
            print("Норма");
    ;

    return 0;