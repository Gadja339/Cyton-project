fun Visocos(int64 year) -> int64:
    if year % 400 == 0:
        return 1;

    if year % 100 == 0:
        return 0;

    if year % 4 == 0:
        return 1;

    return 0;

fun daysInMonth(int64 month, int64 year) -> int64:

    if month == 2:
        if Visocos(year) == 1:
            return 29
        else:
            return 28;
    ;

    if month == 3 or month == 7 or month == 8 or month == 5 or month == 10 or month == 12 or month == 1:
        return 31;

    if month == 4 or month == 6 or month == 9 or month == 11:
        return 30;

    

    return 0;

fun isValidDate(int64 day, int64 month, int64 year) -> int64:
    if year <= 0:
        return 0;

    if month < 1:
        return 0;

    if month > 12:
        return 0;

    if day < 1:
        return 0;

    if day > daysInMonth(month, year):
        return 0;

    return 1;

fun main() -> int64:
    print(isValidDate(31, 2, 2024)) # 1, високосный год

    return 0;