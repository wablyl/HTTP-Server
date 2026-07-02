# Assignment 4 Resources

This directory contains resources for assignment 4:

* `library/` A directory containing the start code library.

* `test_scripts/` A directory containing test scripts for evaluating your
  assignment.

* `load_repo.sh` A script that can be used to load your asgn4 for testing.

* `test_repo.sh` A script that can be used to test your repository.

In the instructions that follow, `{path_to_asgn4_dir}` is the path to
your `asgn4` directory.p

Finally, our tests require that you have another package in your
system, named `net-tools`.  To install it, run:

```
sudo apt install net-tools
```

## Using these Resources

1. Copy the tests and helper functions to your repository using the supplied `load_repo.sh`
   script:

```
./load_repo.sh {path_to_asgn4_dir}
```

where `{path_to_asgn4_dir}` is the path to your asgn4 directory.

2. Go to your `asgn4` directory:

```
cd {path_to_asgn4_dir}
```

3. Make your httpserver binary:

```
make
````

4. execute `test_repo.sh`:

```
./test_repo.sh
```

This command will print out each test and whether it passed or failed.
If the test passed, you will see a message saying "SUCCESS"; if it
fails you will see a message saying "FAILURE".

You can count the number of correct tests by executing:

```
./test_repo.sh | grep "SUCCESS" | wc -l
```

Note that there are several additional hidden tests that will be run for your final submission along with the public tests. The autograder will give you a one-sentence description of what each test does along with indicators if they passed or not. You are recommended to reuse and modify the public tests to look for edge cases that may be tested as part of the hidden tests.

You can execute each
test individually as well.  For example, to execute the test
`test_scripts/test_xxx.sh`, run:

```
./test_scripts/test_xxx.sh
```

You may find it useful to comment out the line `cleanup $new_files` at the bottom of
the test script that you are running in order to see the output files that are
generated.

### Using the Helper Functions:

You can use the static library, `asgn4_helper_funcs.a`, like you would an object file.  That is, when
you build your final executable you will include it in the command
line.  For example, to to build your program, `httpserver`, using your
`httpserver.c` file and the `asgn4_helper_funcs.a` library, you would
execute:

```
clang -o httpserver httpserver.c asgn4_helper_funcs.a
```
