import ctypes

# Based on the dbid enum from libpromises/dbm_api.h
dbs = (
    "classes",   # Deprecated
    "variables", # Deprecated
    "performance",
    "checksums", # Deprecated
    "filestats", # Deprecated
    "changes",
    "observations",
    "state",
    "lastseen",
    "audit",
    "locks",
    "history",
    "measure",
    "static",
    "scalars",
    "windows_registry",
    "cache",
    "license",
    "value",
    "agent_execution",
    "bundles",   # Deprecated
    "packages_installed", # new package promise installed packages list
    "packages_updates",   # new package promise list of available updates
    "cookies", # Enterprise reporting cookies for duplicate host detection
)

def get_db_id(db_name):
    return dbs.index(db_name)


class _SimulationStruct(ctypes.Structure):
    pass

class _FilamentStruct(ctypes.Structure):
    pass

_promises = ctypes.CDLL("libpromises.so.3.0.6")

_promises.SimulateDBLoad.restype = ctypes.POINTER(_SimulationStruct)
_promises.SimulateDBLoad.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_long, ctypes.c_long,
                                     ctypes.c_int, ctypes.c_int, ctypes.c_long, ctypes.c_long,
                                     ctypes.c_long, ctypes.c_long]
def SimulateDBLoad(db_id,
                   read_keys_refresh_s=5,
                   read_min_interval_ms=100,
                   read_max_interval_ms=200,
                   write_sample_size_pct=50,
                   write_prune_interval_s=10,
                   write_min_interval_ms=200,
                   write_max_interval_ms=400,
                   iter_min_interval_ms=1000,
                   iter_max_interval_ms=2000):
    return _promises.SimulateDBLoad(db_id,
                                    read_keys_refresh_s, read_min_interval_ms, read_max_interval_ms,
                                    write_sample_size_pct,
                                    write_prune_interval_s, write_min_interval_ms, write_max_interval_ms,
                                    iter_min_interval_ms, iter_max_interval_ms)

_promises.StopSimulation.restype = None # void
_promises.StopSimulation.argtypes = [ctypes.POINTER(_SimulationStruct)]
def StopSimulation(simulation):
    _promises.StopSimulation(simulation)

_promises.FillUpDB.restype = ctypes.POINTER(_FilamentStruct)
_promises.FillUpDB.argtypes = [ctypes.c_int, ctypes.c_int]
def FillUpDB(db_id, usage):
    return _promises.FillUpDB(db_id, usage)

_promises.RemoveFilament.restype = None # void
_promises.RemoveFilament.argtypes = [ctypes.POINTER(_FilamentStruct)]
def RemoveFilament(filament):
    _promises.RemoveFilament(filament)
