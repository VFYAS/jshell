<code>Makefile</code> commands:
<ol>
  <li> To compile the <code>main.c</code> just type <code>make</code> in your shell. Also <code>make run</code> will
    compile and then run the program.
  This will compile the program with valgrind support </li>
  <li> To run the <code>main.c</code> with valgrind run <code>make valcheck</code>. </li>
  <li> To test the <code>main.c</code> run <code>make test_main</code>. This will run tests, that are located at
    tests_main/tests, and compare the program output with the answers, that are located at tests_main/keys, and compare
    the exit code of the program with the correct one. </li>
  <li> To clean up run <code>make clean</code>. This will delete all the binary files and testing outputs if existed. </li>
</ol>