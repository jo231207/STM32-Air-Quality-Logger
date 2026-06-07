# Coding Style

This project uses the Linux kernel C/C++ coding style as the baseline, with project-specific overrides.

Project rules:

- Use 4 spaces for one indentation level.
- Do not use hard tab characters for indentation.
- Put braces on their own lines:

```c
if (condition)
{
    do_work();
}
```

- Keep formatting automated through `.clang-format`.
- Apply formatting to project-owned code under `Core/`, `FATFS/App/`, and `FATFS/Target/`.
- Do not reformat third-party or vendor code under `Drivers/` or `Middlewares/` unless explicitly required.

## Comments and Documentation

- Public functions exposed through a header file must have a Doxygen comment in the matching `.h`
  file.
- `static` internal functions should have a short Doxygen comment directly above the function in the
  `.c` file.
- Complex logic should have only one or two short code comments near the relevant block. Put the
  broader design explanation in `README.md` or a dedicated document instead of overloading the code.
- Functions that return `1`/`0` must document the meaning of each return value.
- `enum` values and `#define` constants should have short comments beside the value when the meaning
  is not immediately obvious.
- In agent-authored code, `enum` values must be explicitly assigned, such as `= 0`, `= 1`, and
  `= 2`, even when the values are sequential. This keeps debugger/watch-window output easy to map
  back to source code and makes the value mapping clear to readers who are not familiar with C enum
  auto-increment rules.
