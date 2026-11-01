// Runs before main(), so the filesystem is hydrated before Zelda reads saves.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function() {
  FS.mkdir('/saves');
  FS.mount(IDBFS, {}, '/saves');
  addRunDependency('save-data');
  FS.syncfs(true, function(error) {
    if (error) console.warn('Could not load IndexedDB saves:', error);
    removeRunDependency('save-data');
  });
});
