                             //
                             // ram 
                             // ram:0000:0000-ram:0000:03ff
                             //
             assume DF = 0x0  (Default)
       0000:0000 e9  d8  00       JMP        FUN_0000_00db                                    undefined FUN_0000_00db()
                             -- Flow Override: CALL_RETURN (CALL_TERMINATOR)
       0000:0003 e9  13  01       JMP        FUN_0000_0119                                    undefined FUN_0000_0119()
                             -- Flow Override: CALL_RETURN (CALL_TERMINATOR)
       0000:0006 e9  62  01       JMP        FUN_0000_016b                                    undefined FUN_0000_016b()
                             -- Flow Override: CALL_RETURN (CALL_TERMINATOR)
       0000:0009 e9  74  01       JMP        FUN_0000_0180                                    undefined FUN_0000_0180()
                             -- Flow Override: CALL_RETURN (CALL_TERMINATOR)
       0000:000c e9  a6  01       JMP        FUN_0000_01b5                                    undefined FUN_0000_01b5()
                             -- Flow Override: CALL_RETURN (CALL_TERMINATOR)
       0000:000f e9  71  02       JMP        FUN_0000_0283                                    undefined FUN_0000_0283()
                             -- Flow Override: CALL_RETURN (CALL_TERMINATOR)
       0000:0012 e9  00  02       JMP        FUN_0000_0215                                    undefined FUN_0000_0215()
                             -- Flow Override: CALL_RETURN (CALL_TERMINATOR)
                             *************************************************************
                             *                           FUNCTION                         
                             *************************************************************
                             undefined  __cdecl16near  FUN_0000_0015 ()
             undefined         <UNASSIGNED>   <RETURN>
                             FUN_0000_0015                                   XREF[1]:     FUN_0000_0215:0000:021b (c)   
       0000:0015 57              PUSH       DI
       0000:0016 8a  e0           MOV        AH ,AL
       0000:0018 d0  e0           SHL        AL ,0x1
       0000:001a 02  c4           ADD        AL ,AH
       0000:001c 32  e4           XOR        AH ,AH
       0000:001e 8d  3e  36       LEA        DI ,[0x1d36 ]
                 1d
       0000:0022 03  f8           ADD        DI ,AX
       0000:0024 83  3e  30       CMP        word ptr [0x1130 ],0x1
                 11  01
       0000:0029 74  07           JZ         LAB_0000_0032
       0000:002b 8a  05           MOV        AL ,byte ptr [DI ]
       0000:002d 8a  e0           MOV        AH ,AL
       0000:002f eb  04           JMP        LAB_0000_0035
       0000:0031 90              ??         90h
                             LAB_0000_0032                                   XREF[1]:     0000:0029 (j)   
       0000:0032 8b  45  01       MOV        AX ,word ptr [DI  + 0x1 ]
                             LAB_0000_0035                                   XREF[1]:     0000:002f (j)   
       0000:0035 5f              POP        DI
       0000:0036 c3              RET
       0000:0037 56              PUSH       SI
       0000:0038 57              PUSH       DI
       0000:0039 55              PUSH       BP
       0000:003a 8b  ec           MOV        BP ,SP
       0000:003c 83  ec  02       SUB        SP ,0x2
       0000:003f 33  c9           XOR        CX ,CX
       0000:0041 89  0e  96       MOV        word ptr [0x1d96 ],CX
                 1d
       0000:0045 83  3e  30       CMP        word ptr [0x1130 ],0x1
                 11  01
       0000:004a 75  0c           JNZ        LAB_0000_0058
       0000:004c 8d  36  76       LEA        SI ,[0x1d76 ]
                 1d
       0000:0050 8d  3e  86       LEA        DI ,[0x1d86 ]
                 1d
       0000:0054 b1  08           MOV        CL ,0x8
       0000:0056 f3  a5           MOVSW.REP  ES :DI ,SI
                             LAB_0000_0058                                   XREF[1]:     0000:004a (j)   
       0000:0058 8b  76  08       MOV        SI ,word ptr [BP  + 0x8 ]
       0000:005b 8b  ee           MOV        BP ,SI
       0000:005d 83  c6  02       ADD        SI ,0x2
       0000:0060 ac              LODSB      SI
       0000:0061 8a  c8           MOV        CL ,AL
       0000:0063 83  c6  02       ADD        SI ,0x2
                             LAB_0000_0066                                   XREF[1]:     0000:00d2 (j)   
       0000:0066 ad              LODSW      SI
       0000:0067 3b  06  96       CMP        AX ,word ptr [0x1d96 ]
                 1d
       0000:006b 74  65           JZ         LAB_0000_00d2
       0000:006d a3  96  1d       MOV        [0x1d96 ],AX
       0000:0070 51              PUSH       CX
       0000:0071 56              PUSH       SI
       0000:0072 8b  d5           MOV        DX ,BP
       0000:0074 03  d0           ADD        DX ,AX
       0000:0076 8b  f2           MOV        SI ,DX
       0000:0078 ac              LODSB      SI
       0000:0079 8a  c8           MOV        CL ,AL
                             LAB_0000_007b                                   XREF[1]:     0000:00ce (j)   
       0000:007b 51              PUSH       CX
       0000:007c ad              LODSW      SI
       0000:007d 56              PUSH       SI
       0000:007e 03  c2           ADD        AX ,DX
       0000:0080 8b  f0           MOV        SI ,AX
       0000:0082 46              INC        SI
       0000:0083 ac              LODSB      SI
       0000:0084 8a  c8           MOV        CL ,AL
       0000:0086 8b  fe           MOV        DI ,SI
       0000:0088 ac              LODSB      SI
       0000:0089 8a  e8           MOV        CH ,AL
       0000:008b 80  e5  f0       AND        CH ,0xf0
       0000:008e 24  0f           AND        AL ,0xf
       0000:0090 8d  1e  66       LEA        BX ,[0x1d66 ]
                 1d
       0000:0094 83  3e  30       CMP        word ptr [0x1130 ],0x1
                 11  01
       0000:0099 75  0c           JNZ        LAB_0000_00a7
       0000:009b 8d  1e  86       LEA        BX ,[0x1d86 ]
                 1d
       0000:009f 98              CBW
       0000:00a0 03  d8           ADD        BX ,AX
       0000:00a2 c6  07  44       MOV        byte ptr [BX ],0x44
       0000:00a5 2b  d8           SUB        BX ,AX
                             LAB_0000_00a7                                   XREF[1]:     0000:0099 (j)   
       0000:00a7 d7              XLAT       BX
       0000:00a8 24  0f           AND        AL ,0xf
       0000:00aa 0a  c5           OR         AL ,CH
       0000:00ac aa              STOSB      ES :DI
       0000:00ad 32  ed           XOR        CH ,CH
                             LAB_0000_00af                                   XREF[2]:     0000:00c7 (j) ,  0000:00ca (j)   
       0000:00af ac              LODSB      SI
       0000:00b0 0a  c0           OR         AL ,AL
       0000:00b2 74  15           JZ         LAB_0000_00c9
       0000:00b4 8a  e0           MOV        AH ,AL
       0000:00b6 80  e4  0f       AND        AH ,0xf
       0000:00b9 d0  e8           SHR        AL ,0x1
       0000:00bb d0  e8           SHR        AL ,0x1
       0000:00bd d0  e8           SHR        AL ,0x1
       0000:00bf d0  e8           SHR        AL ,0x1
       0000:00c1 d7              XLAT       BX
       0000:00c2 24  f0           AND        AL ,0xf0
       0000:00c4 0a  c4           OR         AL ,AH
       0000:00c6 aa              STOSB      ES :DI
       0000:00c7 eb  e6           JMP        LAB_0000_00af
                             LAB_0000_00c9                                   XREF[1]:     0000:00b2 (j)   
       0000:00c9 aa              STOSB      ES :DI
       0000:00ca e2  e3           LOOP       LAB_0000_00af
       0000:00cc 5e              POP        SI
       0000:00cd 59              POP        CX
       0000:00ce e2  ab           LOOP       LAB_0000_007b
       0000:00d0 5e              POP        SI
       0000:00d1 59              POP        CX
                             LAB_0000_00d2                                   XREF[1]:     0000:006b (j)   
       0000:00d2 e2  92           LOOP       LAB_0000_0066
       0000:00d4 83  c4  02       ADD        SP ,0x2
       0000:00d7 5d              POP        BP
       0000:00d8 5f              POP        DI
       0000:00d9 5e              POP        SI
       0000:00da c3              RET
                             *************************************************************
                             *                           FUNCTION                         
                             *************************************************************
                             undefined  __cdecl16near  FUN_0000_00db ()
             undefined         <UNASSIGNED>   <RETURN>
                             FUN_0000_00db                                   XREF[2]:     0000:0000 (c) , 
                                                                                          FUN_0000_016b:0000:0173 (c)   
       0000:00db b8  04  00       MOV        AX ,0x4
       0000:00de cd  10           INT        0x10
       0000:00e0 1e              PUSH       DS
       0000:00e1 83  3e  30       CMP        word ptr [0x1130 ],0x0
                 11  00
       0000:00e6 74  10           JZ         LAB_0000_00f8
       0000:00e8 bb  01  00       MOV        BX ,0x1
       0000:00eb b4  0b           MOV        AH ,0xb
       0000:00ed cd  10           INT        0x10
       0000:00ef bb  00  01       MOV        BX ,0x100
       0000:00f2 b4  0b           MOV        AH ,0xb
       0000:00f4 cd  10           INT        0x10
       0000:00f6 eb  1c           JMP        LAB_0000_0114
                             LAB_0000_00f8                                   XREF[1]:     0000:00e6 (j)   
       0000:00f8 b8  40  00       MOV        AX ,0x40
       0000:00fb 8e  d8           MOV        DS ,AX
       0000:00fd a1  10  00       MOV        AX ,[0x10 ]=> DAT_0000_0410
       0000:0100 24  cf           AND        AL ,0xcf
       0000:0102 0c  20           OR         AL ,0x20
       0000:0104 a3  10  00       MOV        [0x10 ]=> DAT_0000_0410 ,AX
       0000:0107 b0  1a           MOV        AL ,0x1a
       0000:0109 a2  65  00       MOV        [0x65 ]=> DAT_0000_0465 ,AL
       0000:010c ba  d8  03       MOV        DX ,0x3d8
       0000:010f ee              OUT        DX ,AL
       0000:0110 42              INC        DX
       0000:0111 b0  27           MOV        AL ,0x27
       0000:0113 ee              OUT        DX ,AL
                             LAB_0000_0114                                   XREF[1]:     0000:00f6 (j)   
       0000:0114 1f              POP        DS
       0000:0115 e8  68  00       CALL       FUN_0000_0180                                    undefined FUN_0000_0180()
       0000:0118 c3              RET
                             *************************************************************
                             *                           FUNCTION                         
                             *************************************************************
                             undefined  __cdecl16near  FUN_0000_0119 ()
             undefined         <UNASSIGNED>   <RETURN>
                             FUN_0000_0119                                   XREF[1]:     0000:0003 (c)   
       0000:0119 56              PUSH       SI
       0000:011a 57              PUSH       DI
       0000:011b 55              PUSH       BP
       0000:011c 8b  ec           MOV        BP ,SP
       0000:011e 83  ec  02       SUB        SP ,0x2
       0000:0121 b4  00           MOV        AH ,0x0
       0000:0123 b0  01           MOV        AL ,0x1
       0000:0125 cd  10           INT        0x10
       0000:0127 1e              PUSH       DS
       0000:0128 b8  40  00       MOV        AX ,0x40
       0000:012b 8e  d8           MOV        DS ,AX
       0000:012d b0  08           MOV        AL ,0x8
       0000:012f a2  65  00       MOV        [0x65 ]=> DAT_0000_0465 ,AL
       0000:0132 ba  d8  03       MOV        DX ,0x3d8
       0000:0135 ee              OUT        DX ,AL
       0000:0136 1f              POP        DS
       0000:0137 83  3e  30       CMP        word ptr [0x1130 ],0x1
                 11  01
       0000:013c 75  06           JNZ        LAB_0000_0144
       0000:013e ba  d9  03       MOV        DX ,0x3d9
       0000:0141 b0  01           MOV        AL ,0x1
       0000:0143 ee              OUT        DX ,AL
                             LAB_0000_0144                                   XREF[1]:     0000:013c (j)   
       0000:0144 e8  39  00       CALL       FUN_0000_0180                                    undefined FUN_0000_0180()
       0000:0147 b4  01           MOV        AH ,0x1
       0000:0149 b9  00  10       MOV        CX ,0x1000
       0000:014c cd  10           INT        0x10
       0000:014e b4  02           MOV        AH ,0x2
       0000:0150 32  ff           XOR        BH ,BH
       0000:0152 33  d2           XOR        DX ,DX
       0000:0154 cd  10           INT        0x10
       0000:0156 a0  d1  05       MOV        AL ,[0x5d1 ]
       0000:0159 b4  06           MOV        AH ,0x6
       0000:015b 32  c0           XOR        AL ,AL
       0000:015d 33  c9           XOR        CX ,CX
       0000:015f ba  27  18       MOV        DX ,0x1827
       0000:0162 cd  10           INT        0x10
       0000:0164 83  c4  02       ADD        SP ,0x2
       0000:0167 5d              POP        BP
       0000:0168 5f              POP        DI
       0000:0169 5e              POP        SI
       0000:016a c3              RET
                             *************************************************************
                             *                           FUNCTION                         
                             *************************************************************
                             undefined  __cdecl16near  FUN_0000_016b ()
             undefined         <UNASSIGNED>   <RETURN>
                             FUN_0000_016b                                   XREF[1]:     0000:0006 (c)   
       0000:016b 56              PUSH       SI
       0000:016c 57              PUSH       DI
       0000:016d 55              PUSH       BP
       0000:016e 8b  ec           MOV        BP ,SP
       0000:0170 83  ec  02       SUB        SP ,0x2
       0000:0173 e8  65  ff       CALL       FUN_0000_00db                                    undefined FUN_0000_00db()
       0000:0176 e8  cd  bb       CALL       SUB_0000_bd46
       0000:0179 83  c4  02       ADD        SP ,0x2
       0000:017c 5d              POP        BP
       0000:017d 5f              POP        DI
       0000:017e 5e              POP        SI
       0000:017f c3              RET
                             *************************************************************
                             *                           FUNCTION                         
                             *************************************************************
                             undefined  __cdecl16near  FUN_0000_0180 ()
             undefined         <UNASSIGNED>   <RETURN>
                             FUN_0000_0180                                   XREF[3]:     0000:0009 (c) , 
                                                                                          FUN_0000_00db:0000:0115 (c) , 
                                                                                          FUN_0000_0119:0000:0144 (c)   
       0000:0180 b4  0f           MOV        AH ,0xf
       0000:0182 cd  10           INT        0x10
       0000:0184 33  db           XOR        BX ,BX
       0000:0186 3c  04           CMP        AL ,0x4
       0000:0188 72  0e           JC         LAB_0000_0198
       0000:018a 8a  1e  65       MOV        BL ,byte ptr [0x1365 ]
                 13
       0000:018e 03  1e  67       ADD        BX ,word ptr [0x1367 ]
                 13
       0000:0192 88  1e  65       MOV        byte ptr [0x1365 ],BL
                 13
       0000:0196 eb  0c           JMP        LAB_0000_01a4
                             LAB_0000_0198                                   XREF[1]:     0000:0188 (j)   
       0000:0198 8a  1e  66       MOV        BL ,byte ptr [0x1366 ]
                 13
       0000:019c 03  1e  67       ADD        BX ,word ptr [0x1367 ]
                 13
       0000:01a0 88  1e  66       MOV        byte ptr [0x1366 ],BL
                 13
                             LAB_0000_01a4                                   XREF[1]:     0000:0196 (j)   
       0000:01a4 c7  06  67       MOV        word ptr [0x1367 ],0x0
                 13  00  00
       0000:01aa ba  d4  03       MOV        DX ,0x3d4
       0000:01ad b0  02           MOV        AL ,0x2
       0000:01af ee              OUT        DX ,AL
       0000:01b0 42              INC        DX
       0000:01b1 8a  c3           MOV        AL ,BL
       0000:01b3 ee              OUT        DX ,AL
       0000:01b4 c3              RET
                             *************************************************************
                             *                           FUNCTION                         
                             *************************************************************
                             undefined  __cdecl16near  FUN_0000_01b5 ()
             undefined         <UNASSIGNED>   <RETURN>
                             FUN_0000_01b5                                   XREF[1]:     0000:000c (c)   
       0000:01b5 f6  c4  01       TEST       AH ,0x1
       0000:01b8 74  04           JZ         LAB_0000_01be
       0000:01ba fe  cc           DEC        AH
       0000:01bc fe  c3           INC        BL
                             LAB_0000_01be                                   XREF[1]:     0000:01b8 (j)   
       0000:01be fe  c3           INC        BL
       0000:01c0 80  e3  fe       AND        BL ,0xfe
       0000:01c3 8b  cb           MOV        CX ,BX
       0000:01c5 e8  9e  bc       CALL       SUB_0000_be66
       0000:01c8 8b  f7           MOV        SI ,DI
       0000:01ca e8  57  bc       CALL       SUB_0000_be24
       0000:01cd 8b  d9           MOV        BX ,CX
       0000:01cf 8b  eb           MOV        BP ,BX
       0000:01d1 81  e5  ff       AND        BP ,0xff
                 00
       0000:01d5 81  c5  a0       ADD        BP ,0xa0
                 00
       0000:01d9 d0  eb           SHR        BL ,0x1
       0000:01db 33  d2           XOR        DX ,DX
       0000:01dd 8a  d3           MOV        DL ,BL
       0000:01df 81  c2  00       ADD        DX ,0x2000
                 20
       0000:01e3 1e              PUSH       DS
       0000:01e4 06              PUSH       ES
       0000:01e5 8e  06  71       MOV        ES ,word ptr [0x1371 ]
                 13
       0000:01e9 8e  1e  6f       MOV        DS ,word ptr [0x136f ]
                 13
       0000:01ed 33  c9           XOR        CX ,CX
                             LAB_0000_01ef                                   XREF[2]:     0000:020a (j) ,  0000:0210 (j)   
       0000:01ef 8a  cb           MOV        CL ,BL
                             LAB_0000_01f1                                   XREF[1]:     0000:0200 (j)   
       0000:01f1 ad              LODSW      SI
       0000:01f2 25  0f  0f       AND        AX ,0xf0f
       0000:01f5 d0  e0           SHL        AL ,0x1
       0000:01f7 d0  e0           SHL        AL ,0x1
       0000:01f9 d0  e0           SHL        AL ,0x1
       0000:01fb d0  e0           SHL        AL ,0x1
       0000:01fd 0a  c4           OR         AL ,AH
       0000:01ff aa              STOSB      ES :DI
       0000:0200 e2  ef           LOOP       LAB_0000_01f1
       0000:0202 fe  cf           DEC        BH
       0000:0204 74  0c           JZ         LAB_0000_0212
       0000:0206 2b  f5           SUB        SI ,BP
       0000:0208 2b  fa           SUB        DI ,DX
       0000:020a 73  e3           JNC        LAB_0000_01ef
       0000:020c 81  c7  b0       ADD        DI ,0x3fb0
                 3f
       0000:0210 eb  dd           JMP        LAB_0000_01ef
                             LAB_0000_0212                                   XREF[1]:     0000:0204 (j)   
       0000:0212 07              POP        ES
       0000:0213 1f              POP        DS
       0000:0214 c3              RET
                             *************************************************************
                             *                           FUNCTION                         
                             *************************************************************
                             undefined  __cdecl16near  FUN_0000_0215 ()
             undefined         <UNASSIGNED>   <RETURN>
                             FUN_0000_0215                                   XREF[1]:     0000:0012 (c)   
       0000:0215 06              PUSH       ES
       0000:0216 8e  06  71       MOV        ES ,word ptr [0x1371 ]
                 13
       0000:021a 92              XCHG       AX ,DX
       0000:021b e8  f7  fd       CALL       FUN_0000_0015                                    undefined FUN_0000_0015()
       0000:021e 92              XCHG       AX ,DX
       0000:021f d0  e6           SHL        DH ,0x1
       0000:0221 d0  e6           SHL        DH ,0x1
       0000:0223 d0  e6           SHL        DH ,0x1
       0000:0225 d0  e6           SHL        DH ,0x1
       0000:0227 0a  d6           OR         DL ,DH
                             LAB_0000_0229                                   XREF[1]:     0000:025e (j)   
       0000:0229 8b  c8           MOV        CX ,AX
       0000:022b e8  34  00       CALL       FUN_0000_0262                                    undefined FUN_0000_0262()
       0000:022e 8b  c1           MOV        AX ,CX
       0000:0230 32  ed           XOR        CH ,CH
       0000:0232 8a  cb           MOV        CL ,BL
       0000:0234 f6  c4  01       TEST       AH ,0x1
       0000:0237 74  05           JZ         LAB_0000_023e
       0000:0239 fe  c9           DEC        CL
       0000:023b 74  1b           JZ         LAB_0000_0258
       0000:023d 47              INC        DI
                             LAB_0000_023e                                   XREF[1]:     0000:0237 (j)   
       0000:023e 50              PUSH       AX
       0000:023f 02  e3           ADD        AH ,BL
       0000:0241 fe  cc           DEC        AH
       0000:0243 f6  c4  01       TEST       AH ,0x1
       0000:0246 75  07           JNZ        LAB_0000_024f
       0000:0248 57              PUSH       DI
       0000:0249 e8  16  00       CALL       FUN_0000_0262                                    undefined FUN_0000_0262()
       0000:024c fe  c9           DEC        CL
       0000:024e 5f              POP        DI
                             LAB_0000_024f                                   XREF[1]:     0000:0246 (j)   
       0000:024f e3  06           JCXZ       LAB_0000_0257
       0000:0251 d1  e9           SHR        CX ,0x1
       0000:0253 8a  c2           MOV        AL ,DL
       0000:0255 f3  aa           STOSB.REP  ES :DI
                             LAB_0000_0257                                   XREF[1]:     0000:024f (j)   
       0000:0257 58              POP        AX
                             LAB_0000_0258                                   XREF[1]:     0000:023b (j)   
       0000:0258 fe  cf           DEC        BH
       0000:025a 74  04           JZ         LAB_0000_0260
       0000:025c fe  c8           DEC        AL
       0000:025e eb  c9           JMP        LAB_0000_0229
                             LAB_0000_0260                                   XREF[1]:     0000:025a (j)   
       0000:0260 07              POP        ES
       0000:0261 c3              RET
                             *************************************************************
                             *                           FUNCTION                         
                             *************************************************************
                             undefined  __cdecl16near  FUN_0000_0262 ()
             undefined         <UNASSIGNED>   <RETURN>
                             FUN_0000_0262                                   XREF[2]:     FUN_0000_0215:0000:022b (c) , 
                                                                                          FUN_0000_0215:0000:0249 (c)   
       0000:0262 8b  f3           MOV        SI ,BX
       0000:0264 e8  bd  bb       CALL       SUB_0000_be24
       0000:0267 8b  de           MOV        BX ,SI
       0000:0269 b6  0f           MOV        DH ,0xf
       0000:026b f6  c4  01       TEST       AH ,0x1
       0000:026e 74  02           JZ         LAB_0000_0272
       0000:0270 f6  d6           NOT        DH
                             LAB_0000_0272                                   XREF[1]:     0000:026e (j)   
       0000:0272 26  8a  05       MOV        AL ,byte ptr ES :[DI ]
       0000:0275 22  c6           AND        AL ,DH
       0000:0277 8a  e2           MOV        AH ,DL
       0000:0279 f6  d6           NOT        DH
       0000:027b 22  e6           AND        AH ,DH
       0000:027d 0a  c4           OR         AL ,AH
       0000:027f 26  88  05       MOV        byte ptr ES :[DI ],AL
       0000:0282 c3              RET
                             *************************************************************
                             *                           FUNCTION                         
                             *************************************************************
                             undefined  __cdecl16near  FUN_0000_0283 ()
             undefined         <UNASSIGNED>   <RETURN>
                             FUN_0000_0283                                   XREF[1]:     0000:000f (c)   
       0000:0283 8d  3e  7b       LEA        DI ,[0x137b ]
                 13
       0000:0287 b8  00  00       MOV        AX ,0x0
       0000:028a 26  3b  05       CMP        AX ,word ptr ES :[DI ]
       0000:028d 74  1c           JZ         LAB_0000_02ab
       0000:028f c6  06  65       MOV        byte ptr [0x1365 ],0x2d
                 13  2d
       0000:0294 c6  06  66       MOV        byte ptr [0x1366 ],0x2d
                 13  2d
       0000:0299 ba  64  00       MOV        DX ,0x64
                             LAB_0000_029c                                   XREF[1]:     0000:02a9 (j)   
       0000:029c b9  02  00       MOV        CX ,0x2
                             LAB_0000_029f                                   XREF[1]:     0000:02a3 (j)   
       0000:029f ab              STOSW      ES :DI
       0000:02a0 05  00  20       ADD        AX ,0x2000
       0000:02a3 e2  fa           LOOP       LAB_0000_029f
       0000:02a5 2d  b0  3f       SUB        AX ,0x3fb0
       0000:02a8 4a              DEC        DX
       0000:02a9 75  f1           JNZ        LAB_0000_029c
                             LAB_0000_02ab                                   XREF[1]:     0000:028d (j)   
       0000:02ab c3              RET
