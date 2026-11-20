"""Jobs spread over worker processes: fresh processes for every batch, a failed job run once more.

Why fresh processes. On 2026-11-20 worker processes that had lived through many jobs failed in five builds out
of five: four lost a worker with no error, one raised `ZeroDivisionError: float modulo` on `x % 1.0`, which
correct arithmetic cannot do. The same jobs never failed in a process started for them: one job run 96 times
side by side on 16 cores gave the same number to the last bit each time. The cause is not found (the PC was busy
with other work; nothing is in Windows' logs). Until it is, no worker outlives its batch.

Why a job is run again. The runs are seeded: a job that fails and then passes was failed by the PC, and gives
what it would have given. One that fails twice is a fault in the code and is raised.
"""

import os
from concurrent.futures import ProcessPoolExecutor
from concurrent.futures.process import BrokenProcessPool


class Workers:
    """Runs batches of jobs on half the PC's cores."""

    def __init__(self):
        self.count = max(1, (os.cpu_count() or 2) // 2)

    def __enter__(self):
        return self

    def __exit__(self, *_):
        return None

    def attempt(self, function, jobs: list, indexes: list) -> dict:
        """One pass over some of the jobs in fresh processes: {index: ("ok", result) or ("failed", the error)}."""
        outcome = {}
        with ProcessPoolExecutor(max_workers=min(self.count, len(indexes))) as pool:
            futures = [(index, pool.submit(function, jobs[index])) for index in indexes]
            for index, future in futures:
                try:
                    outcome[index] = ("ok", future.result())
                except BrokenProcessPool as error:
                    outcome[index] = ("failed", error)
                except Exception as error:      # noqa: BLE001  a worker's failure, judged below: run again or raised
                    outcome[index] = ("failed", error)
        return outcome

    def map(self, function, jobs: list) -> list:
        """
        Run a function on each job.

        Args:
            function: a module-level function of one job.
            jobs: the jobs.

        Returns:
            each job's result, in the jobs' order.

        Raises:
            whatever a job raised on both of its runs.
        """
        if not jobs:
            return []
        first = self.attempt(function, jobs, list(range(len(jobs))))
        failed = [index for index, (state, _) in first.items() if state == "failed"]
        if failed:
            kinds = sorted({type(first[index][1]).__name__ for index in failed})
            print(f"  {len(failed)} of {len(jobs)} jobs failed ({', '.join(kinds)}) and are run again", flush=True)
            second = self.attempt(function, jobs, failed)
            for index, (state, value) in second.items():
                if state == "failed":
                    raise value
            first.update(second)
        return [first[index][1] for index in range(len(jobs))]
