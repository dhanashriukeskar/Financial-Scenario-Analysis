@echo off
REM Windows (MSYS2/MinGW g++). Run from this folder in PowerShell or CMD.
if not exist bin mkdir bin
set F=-O2 -std=c++17 -Wall -Wextra
g++ %F% sequential\sequential.cpp -o bin\sequential.exe || exit /b 1
g++ %F% sequential\test_sequential.cpp -o bin\test_sequential.exe || exit /b 1
g++ %F% -fopenmp parallel\parallel.cpp -o bin\parallel.exe || exit /b 1
g++ %F% -fopenmp verify\verify.cpp -o bin\verify.exe || exit /b 1
g++ %F% -fopenmp benchmark\benchmark.cpp -o bin\benchmark.exe || exit /b 1
echo === Unit tests ===
bin\test_sequential.exe
echo === Sequential N=10000 M=200 ===
bin\sequential.exe 10000 200
for %%T in (1 2 4 8) do (
  echo === Parallel threads=%%T static ===
  bin\parallel.exe 10000 200 %%T static
)
echo === Correctness check ===
bin\verify.exe 1000 200
echo.
echo Thread study: bin\benchmark.exe threads 200 7 results\my_benchmark_threads.csv
