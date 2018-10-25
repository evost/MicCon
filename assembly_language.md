| №  | command | arg 1      | arg 2      | note                                     |
| ---| ------- | ---------- | ---------- | ---------------------------------------- |
| 0  | nop     | -          | -          | do nothing                               |
| 1  | ret     | -          | -          | exit the subroutine                      |
| 2  | clf     | -          | -          | close an open file                       |
| 3  | end     | -          | -          | ends the program                         |
| 4  | clr     | -          | -          | clear screen and set X=0, Y=0            |
| 5  | xch     | var1       | var2       | swap(var1, var2)                         |
| 6  | add     | var1       | var2/int   | var1 += var2                             |
| 7  | sub     | var1       | var2/int   | var1 -= var2                             |
| 8  | mul     | var1       | var2/int   | var1 *= var2                             |
| 9  | div     | var1       | var2/int   | var1 /= var2                             |
| 10 | mod     | var1       | var2/int   | var1 %= var2                             |
| 11 | pow     | var1       | var2/int   | var1 = var1 ^ var2                       |
| 12 | mov     | var1       | var2/int   | var1 = var2                              |
| 13 | orl     | var1       | var2/int   | var1 \|= var2                            |
| 14 | and     | var1       | var2/int   | var1 &= var2                             |
| 15 | not     | var1       | var2/int   | var1 ~= var2                             |
| 16 | xor     | var1       | var2/int   | var1 ^= var2                             |
| 17 | neg     | var1       | var2/int   | var1 = -var2                             |
| 18 | rol     | var1       | var2/int   | var1 = var1 << var2 + var1 >> (16 - var2)|
| 19 | ror     | var1       | var2/int   | var1 = var1 >> var2 + var1 << (16 - var2)|
| 20 | shl     | var1       | var2/int   | var1 = var1 << var2                      |
| 21 | shr     | var1       | var2/int   | var1 = var1 >> var2                      |
| 22 | rnd     | var1       | var2/int   | var1 = random(var2)                      |
| 23 | rci     | var1       | var2/int   | read var2 integers from console          |
| 24 | rcc     | var1       | var2/int   | read var2 characters from console        |
| 25 | wci     | var1       | var2/int   | write var2 integers to console           |
| 26 | wcc     | var1       | var2/int   | write var2 characters to console         |
| 27 | rfi     | var1       | var2/int   | read var2 integers from file             |
| 28 | rfc     | var1       | var2/int   | read var2 characters from file           |
| 29 | wfi     | var1       | var2/int   | write var2 integers to file              |
| 30 | wfc     | var1       | var2/int   | write var2 characters to file            |
| 31 | rdd     | var1       | var2/int   | var1 = digitalRead(var2)                 |
| 32 | rda     | var1       | var2/int   | var1 = analogRead(var2)                  |
| 33 | wfd     | var1       | var2/int   | digitalWrite(var1, var2)                 |
| 34 | wfa     | var1       | var2/int   | analogWrite(var1, var2)                  |
| 35 | wsd     | var1       | var2/int   | digitalWrite(var2, var1)                 |
| 36 | wsa     | var1       | var2/int   | analogWrite(var2, var1)                  |
| 37 | stx     | var1/int   | -          | cursorX = var1 * LCD.getFontXsize()      |
| 38 | sty     | var1/int   | -          | cursorY = var1 * LCD.getFontYsize()      |
| 39 | stc     | var1/int   | -          | set background and cursor color          |
| 40 | pni     | var1/int   | -          | pinMode(var1, INPUT)                     |
| 41 | pno     | var1/int   | -          | pinMode(var1, OUTPUT)                    |
| 42 | wsr     | var1/int   | -          | Serial.Write(var1)                       |
| 43 | dly     | var1/int   | -          | delay(var1)                              |
| 44 | rsr     | var1       | -          | var1 = Serial.Read()                     |
| 45 | inc     | var1       | -          | var1++                                   |
| 46 | dec     | var1       | -          | var1--                                   |
| 47 | ofr     | var1       | -          | open file to read                        |
| 48 | ofw     | var1       | -          | open file to rewrite                     |
| 49 | ofa     | var1       | -          | open file to addition                    |
| 50 | mkd     | var1       | -          | make directory                           |
| 51 | rmd     | var1       | -          | remove directory                         |
| 52 | rmf     | var1       | -          | remove file                              |
| 53 | ife     | var1       | label      | if var1==0 go to label                   |
| 54 | ifn     | var1       | label      | if var1!=0 go to label                   |
| 55 | ifb     | var1       | label      | if var1>0 go to label                    |
| 56 | ifs     | var1       | label      | if var1<0 go to label                    |
| 57 | jmp     | label      | -          | go to label                              |
| 58 | cal     | label      | -          | call subroutine label                    |
| -- | ------- | ---------- | ---------- | ---------------------------------------- |
| 59 | lab     | new_label  | -          | declaration of label "new_label"         |
| 60 | prс     | new_label  | -          | declaration of subroutine "new_label"    |
| 61 | dat     | new_var    | uint       | declaration of array[uint]               |
