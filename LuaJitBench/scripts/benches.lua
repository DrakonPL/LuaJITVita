-- The benches that the runner runs, in order. Each name is a module in scripts/bench/.
-- profile: the bench that the runner samples with jit.profile after the runs.
return {
  "trivial",
  "std.fib",
  "std.nbody",
  "std.spectral_norm",
  "std.mandelbrot",
  "std.fannkuch",
  "std.binary_trees",
  "alloc_tables",
  "alloc_strings",
  "std.ray",
  "c_call_capi",
  "c_call_ffi",
  "c_callback_ffi",
  profile = "std.ray",
}
