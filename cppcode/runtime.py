import subprocess
import time
import sys

def benchmark_exe(exe_path, args, runs=10):
    runtimes = []

    for i in range(runs):
        start = time.perf_counter()

        subprocess.run(
            [exe_path] + args,
            stdout=subprocess.DEVNULL,  # remove if you want program output
            stderr=subprocess.DEVNULL,
            check=True
        )

        end = time.perf_counter()
        runtime = end - start
        runtimes.append(runtime)

        print(f"Run {i + 1}: {runtime:.6f} seconds")

    avg = sum(runtimes) / runs
    print("\nAverage runtime:", f"{avg:.6f} seconds")


if __name__ == "__main__":
    # Example usage:
    # python benchmark.py ./main.exe 334 12 3
    if len(sys.argv) < 2:
        print("Usage: python benchmark.py <exe_path> [args...]")
        sys.exit(1)

    exe_path = sys.argv[1]
    args = sys.argv[2:]

    benchmark_exe(exe_path, args)
