
.section .note.crashpad.test,"a",%note
.align 2
.type testnote, %object
testnote:
1:  .long 5f-4f  // namesz
2:  .long 8f-7f  // descsz
3:  .long 1      // type
4:  .ascii "Crashpad\0"  // name
5:
6:  .ascii "\0\0\0"  // padding
7:  .long 42         // desc
8:
  .size testnote, .-testnote
