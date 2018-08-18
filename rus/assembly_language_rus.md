| №  | command | arg 1      | arg 2      | note                                     |
| ---| ------- | ---------- | ---------- | -----------------------------------------|
| 0  | nop     | -          | -          | ничего не делать                         |
| 1  | ret     | -          | -          | выход из подпрограммы                    |
| 2  | clf     | -          | -          | закрыть открытый файл                    |
| 3  | end     | -          | -          | завершает программу                      |
| 4  | clr     | -          | -          | очистить экран и установить X=0, Y=0     |
| 5  | xch     | var1       | var2       | swap(var1, var2)                         |
| 6  | add     | var1       | var2/int   | var1 += var2                             |
| 7  | sub     | var1       | var2/int   | var1 -= var2                             |
| 8  | mul     | var1       | var2/int   | var1 *= var2                             |
| 9  | div     | var1       | var2/int   | var1 /= var2                             |
| 10 | mod     | var1       | var2/int   | var1 %= var2                             |
| 11 | pow     | var1       | var2/int   | var1 = var1 ^ var2                       |
| 12 | mov     | var1       | var2/int   | var1 = var2                              |
| 13 | orl     | var1       | var2/int   | var1 |= var2                             |
| 14 | and     | var1       | var2/int   | var1 &= var2                             |
| 15 | not     | var1       | var2/int   | var1 ~= var2                             |
| 16 | xor     | var1       | var2/int   | var1 ^= var2                             |
| 17 | neg     | var1       | var2/int   | var1 = -var2                             |
| 18 | rol     | var1       | var2/int   | var1 = var1 << var2 + var1 >> (16 - var2)|
| 19 | ror     | var1       | var2/int   | var1 = var1 >> var2 + var1 << (16 - var2)|
| 20 | shl     | var1       | var2/int   | var1 = var1 << var2                      |
| 21 | shr     | var1       | var2/int   | var1 = var1 >> var2                      |
| 22 | rnd     | var1       | var2/int   | var1 = random(var2)                      |
| 23 | rci     | var1       | var2/int   | считать с консоли var2 целых             |
| 24 | rcc     | var1       | var2/int   | считать с консоли var2 символов          |
| 25 | wci     | var1       | var2/int   | вывести в консоль var2 целых             |
| 26 | wcc     | var1       | var2/int   | вывести в консоль var2 символов          |
| 27 | rfi     | var1       | var2/int   | считать с файла var2 целых               |
| 28 | rfc     | var1       | var2/int   | считать с файла var2 символов            |
| 29 | wfi     | var1       | var2/int   | вывести в файл var2 целых                |
| 30 | wfc     | var1       | var2/int   | вывести в файл var2 символов             |
| 31 | rdd     | var1       | var2/int   | var1 = digitalRead(var2)                 |
| 32 | rda     | var1       | var2/int   | var1 = analogRead(var2)                  |
| 33 | wfd     | var1       | var2/int   | digitalWrite(var1, var2)                 |
| 34 | wfa     | var1       | var2/int   | analogWrite(var1, var2)                  |
| 35 | wsd     | var1       | var2/int   | digitalWrite(var2, var1)                 |
| 36 | wsa     | var1       | var2/int   | analogWrite(var2, var1)                  |
| 37 | stx     | var1/int   | -          | cursorX = var1 * LCD.getFontXsize()      |
| 38 | sty     | var1/int   | -          | cursorY = var1 * LCD.getFontYsize()      |
| 39 | stc     | var1/int   | -          | устанавливает цвет фона и курсора        |
| 40 | pni     | var1/int   | -          | pinMode(var1, INPUT)                     |
| 41 | pno     | var1/int   | -          | pinMode(var1, OUTPUT)                    |
| 42 | wsr     | var1/int   | -          | Serial.Write(var1)                       |
| 43 | rsr     | var1       | -          | var1 = Serial.Read()                     |
| 44 | inc     | var1       | -          | var1++                                   |
| 45 | dec     | var1       | -          | var1--                                   |
| 46 | ofr     | var1       | -          | открыть файл на чтение                   |
| 47 | ofw     | var1       | -          | открыть файл на запись                   |
| 48 | ofa     | var1       | -          | открыть файл на дополнение               |
| 49 | mkd     | var1       | -          | создать каталог                          |
| 50 | rmd     | var1       | -          | удалить каталог                          |
| 51 | rmf     | var1       | -          | удалить файл                             |
| 52 | ife     | var1       | label      | если равно нулю перейти по метке         |
| 53 | ifn     | var1       | label      | если не равно нулю перейти по метке      |
| 54 | ifb     | var1       | label      | если больше нуля перейти по метке        |
| 55 | ifs     | var1       | label      | если меньше нуля перейти по метке        |
| 56 | jmp     | label      | -          | безусловный переход по метке lab         |
| 57 | cal     | label      | -          | вызвать подпрограмму по метке lab        |
| -- | ------- | ---------- | ---------- | -----------------------------------------|
| 58 | lab     | new_label  | -          | объявление метки new_lab                 |
| 59 | prс     | new_label  | -          | объявление подпрограммы new_lab          |
| 60 | dat     | new_var    | uint       | объявление массива из num>0 целых        |
