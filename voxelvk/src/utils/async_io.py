
from concurrent.futures import ThreadPoolExecutor
_pool = ThreadPoolExecutor(max_workers=4)
def io_submit(fn, *args, **kwargs):
    return _pool.submit(fn, *args, **kwargs)
