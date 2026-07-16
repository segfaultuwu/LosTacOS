struct Command {
  const char *name;
  const char *description;
  void (*handler)(char *args);
};
