import importlib, sys
sys.stdout.reconfigure(encoding='utf-8')
for m in ['capstone', 'PIL', 'numpy', 'unicorn', 'pefile']:
    try:
        mod = importlib.import_module(m)
        print(f'{m:10s} OK       {getattr(mod, "__version__", "?")}')
    except ImportError:
        print(f'{m:10s} MISSING')
