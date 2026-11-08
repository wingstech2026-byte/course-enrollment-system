/*
 * app.js - user interface of the browser edition.
 * All user-supplied text is inserted with textContent / createTextNode (never innerHTML).
 */
(function () {
  'use strict';

  const { EnrollmentSystem, statusOf } = window.EMSLogic;
  const storage = window.EMSStorage;

  const TITLE = 'Course Enrollment Management System';
  const $ = (selector) => document.querySelector(selector);
  const collator = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });

  const system = new EnrollmentSystem();
  let lastAction = 'Ready.';

  /** Tiny DOM builder: h('tr', { class: 'x', onclick: fn }, child, 'text'). */
  function h(tag, props, ...children) {
    const el = document.createElement(tag);
    for (const [key, value] of Object.entries(props || {})) {
      if (value === false || value == null) continue;
      if (key === 'class') el.className = value;
      else if (key.startsWith('on')) el.addEventListener(key.slice(2), value);
      else el.setAttribute(key, value === true ? '' : value);
    }
    for (const child of children.flat()) el.append(child);
    return el;
  }

  const plural = (n, one, many) => `${n} ${n === 1 ? one : many}`;

  // ===========================================================================
  // Sortable tables
  // ===========================================================================

  /** Creates the header of a table and returns its state object. */
  function makeTable(selector, columns, options = {}) {
    const table = {
      el: $(selector), columns, rows: [], sort: { col: 0, asc: true },
      selectable: !!options.selectable, selectedKey: null, onSelect: options.onSelect || (() => {}),
      emptyText: options.emptyText || 'Nothing to show.',
    };
    const headRow = h('tr');
    columns.forEach((title, index) => {
      const button = h('button', { type: 'button', title: `Sort by ${title}` }, title, h('span', { class: 'arrow', 'aria-hidden': 'true' }, '▲'));
      button.addEventListener('click', () => {
        if (table.sort.col === index) table.sort.asc = !table.sort.asc;
        else table.sort = { col: index, asc: true };
        paint(table);
      });
      headRow.append(h('th', { scope: 'col', 'aria-sort': 'none' }, button));
    });
    table.el.tHead.append(headRow);
    return table;
  }

  /** Sorts the rows of `table` by its sort state and redraws the body. */
  function paint(table) {
    const { col, asc } = table.sort;
    table.rows.sort((a, b) => (asc ? 1 : -1) * collator.compare(a.cells[col], b.cells[col]));

    table.el.tHead.querySelectorAll('th').forEach((th, index) => {
      th.setAttribute('aria-sort', index === col ? (asc ? 'ascending' : 'descending') : 'none');
      th.querySelector('.arrow').textContent = asc ? '▲' : '▼';
    });

    const body = table.el.tBodies[0];
    body.replaceChildren();
    if (table.rows.length === 0) {
      body.append(h('tr', { class: 'empty' }, h('td', { colspan: table.columns.length }, table.emptyText)));
      return;
    }
    for (const row of table.rows) {
      const tr = h('tr', { class: row.tag ? `row-${row.tag}` : '' }, row.cells.map((text) => h('td', {}, text)));
      if (table.selectable) {
        const selected = row.key === table.selectedKey;
        tr.tabIndex = 0;
        tr.setAttribute('aria-selected', String(selected));
        const choose = () => { table.selectedKey = row.key; paint(table); table.onSelect(row.key); focusRow(table, row.key); };
        tr.addEventListener('click', choose);
        tr.addEventListener('keydown', (event) => {
          if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); choose(); }
          else if (event.key === 'ArrowDown' && tr.nextElementSibling) tr.nextElementSibling.focus();
          else if (event.key === 'ArrowUp' && tr.previousElementSibling) tr.previousElementSibling.focus();
        });
      }
      tr.dataset.key = row.key;
      body.append(tr);
    }
  }

  function focusRow(table, key) {
    const tr = [...table.el.tBodies[0].rows].find((r) => r.dataset.key === key);
    if (tr) tr.focus();
  }

  /** Replaces the rows of `table`, keeping the selection if that row still exists. */
  function setRows(table, rows) {
    table.rows = rows;
    if (table.selectedKey !== null && !rows.some((r) => r.key === table.selectedKey)) table.selectedKey = null;
    paint(table);
  }

  const tblStudents = makeTable('#tbl-students', ['Student ID', 'Name', 'Courses Taken'], { emptyText: 'No students match.' });
  const tblCourses = makeTable('#tbl-courses', ['Course ID', 'Name', 'Capacity', 'Enrolled', 'Remaining', 'Status'], { emptyText: 'No courses match.' });
  const tblStudentCourses = makeTable('#tbl-student-courses', ['Course ID', 'Course Name', 'Seats'], {
    selectable: true, emptyText: 'Choose a student to see their courses.', onSelect: () => updateButtons(),
  });
  const tblCourseStudents = makeTable('#tbl-course-students', ['Student ID', 'Student Name'], { emptyText: 'Choose a course to see its students.' });

  // ===========================================================================
  // Refreshing the display
  // ===========================================================================

  const matches = (filter, ...texts) => !filter || texts.some((t) => t.toLowerCase().includes(filter));

  function refreshStudents() {
    const filter = $('#stu-search').value.trim().toLowerCase();
    setRows(tblStudents, system.students
      .filter((s) => matches(filter, s.id, s.name))
      .map((s) => ({ key: s.id, cells: [s.id, s.name, String(system.courseCountOfStudent(s.id))] })));
  }

  function refreshCourses() {
    const filter = $('#crs-search').value.trim().toLowerCase();
    const label = { open: 'Open', almost: 'Almost full', full: 'Full' };
    setRows(tblCourses, system.courses
      .filter((c) => matches(filter, c.id, c.name))
      .map((c) => {
        const enrolled = system.enrolledCount(c.id);
        const status = statusOf(enrolled, c.capacity);
        return {
          key: c.id, tag: status,
          cells: [c.id, c.name, String(c.capacity), String(enrolled), String(Math.max(0, c.capacity - enrolled)), label[status]],
        };
      }));
  }

  /** Refills a <select>, keeping the previous choice when it still exists. */
  function fillSelect(select, placeholder, options) {
    const previous = select.value;
    select.replaceChildren(h('option', { value: '' }, placeholder));
    for (const o of options) select.append(h('option', { value: o.value }, o.label));
    select.value = options.some((o) => o.value === previous) ? previous : '';
  }

  function refreshCombos() {
    const students = [...system.students].sort((a, b) => collator.compare(a.id, b.id));
    const courses = [...system.courses].sort((a, b) => collator.compare(a.id, b.id));
    const courseOptions = courses.map((c) => {
      const left = system.remainingPlaces(c.id);
      return { value: c.id, label: `${c.id} - ${c.name}  (${left > 0 ? `${left} left` : 'full'})` };
    });
    fillSelect($('#enr-student'), '— choose a student —', students.map((s) => ({ value: s.id, label: `${s.id} - ${s.name}` })));
    fillSelect($('#enr-course'), '— choose a course —', courseOptions);
    fillSelect($('#enr-view-course'), '— choose a course —', courseOptions);
  }

  function refreshStudentCourses() {
    const studentId = $('#enr-student').value;
    setRows(tblStudentCourses, studentId
      ? system.coursesOfStudent(studentId).map((c) => ({
          key: c.id, cells: [c.id, c.name, `${system.enrolledCount(c.id)} / ${c.capacity}`],
        }))
      : []);
  }

  function refreshCourseStudents() {
    const courseId = $('#enr-view-course').value;
    const course = courseId ? system.findCourse(courseId) : null;
    const summary = $('#enr-summary');
    const bar = $('#enr-bar');

    if (!course) {
      summary.textContent = 'Select a course to see its enrolled students.';
      bar.className = 'bar';
      bar.firstElementChild.style.width = '0%';
      bar.setAttribute('aria-valuenow', '0');
      setRows(tblCourseStudents, []);
      return;
    }
    const enrolled = system.enrolledCount(courseId);
    const remaining = system.remainingPlaces(courseId);
    summary.textContent = `Enrolled ${enrolled} / ${course.capacity} — ${plural(remaining, 'place', 'places')} remaining`;

    const percent = Math.min(100, Math.round((enrolled / course.capacity) * 100));
    const status = statusOf(enrolled, course.capacity);
    bar.className = `bar ${status === 'open' ? '' : status}`.trim();
    bar.firstElementChild.style.width = `${percent}%`;
    bar.setAttribute('aria-valuenow', String(percent));
    setRows(tblCourseStudents, system.studentsInCourse(courseId).map((s) => ({ key: s.id, cells: [s.id, s.name] })));
  }

  function updateButtons() {
    const studentId = $('#enr-student').value;
    $('#btn-enroll').disabled = !(studentId && $('#enr-course').value);
    $('#btn-withdraw').disabled = !(studentId && tblStudentCourses.selectedKey !== null);
  }

  function updateStatusBar() {
    $('#st-students').textContent = `Students: ${system.students.length}`;
    $('#st-courses').textContent = `Courses: ${system.courses.length}`;
    $('#st-enrollments').textContent = `Enrollments: ${system.enrollments.length}`;
    $('#st-last').textContent = lastAction;
  }

  function updateTitle() {
    document.title = system.modified ? `${TITLE} *` : TITLE;
    $('#unsaved').hidden = !system.modified;
  }

  function setStatus(text) {
    lastAction = text;
    updateStatusBar();
  }

  /** Every list, drop-down and counter is refreshed after any change. */
  function refreshAll() {
    refreshStudents();
    refreshCourses();
    refreshCombos();
    refreshStudentCourses();
    refreshCourseStudents();
    updateButtons();
    updateStatusBar();
    updateTitle();
  }

  // ===========================================================================
  // Message / confirmation dialog (the browser version of MessageBox)
  // ===========================================================================

  /**
   * Shows a modal message. Resolves with the `value` of the chosen button, or 'cancel'
   * when dismissed with Esc.
   */
  function showMessage({ title, text, kind = 'info', buttons = [{ label: 'OK', value: 'ok', primary: true }], focus = 0 }) {
    const dialog = $('#dlg-message');
    $('#msg-title').textContent = title;
    const body = $('#msg-text');
    body.textContent = text;
    body.className = `msg-text ${kind}`;

    const row = $('#msg-buttons');
    row.replaceChildren();
    buttons.forEach((b) => row.append(
      h('button', { type: 'button', class: `btn${b.primary ? ' primary' : ''}`, onclick: () => dialog.close(b.value) }, b.label)
    ));

    dialog.returnValue = '';
    return new Promise((resolve) => {
      dialog.addEventListener('close', () => resolve(dialog.returnValue || 'cancel'), { once: true });
      dialog.showModal();
      row.children[focus].focus();
    });
  }

  const showError = (title, text) => showMessage({ title, text, kind: 'error' });
  const confirmYesNo = async (title, text) =>
    (await showMessage({
      title, text,
      buttons: [{ label: 'Yes', value: 'yes', primary: true }, { label: 'No', value: 'no' }],
      focus: 1, // default answer is "No"
    })) === 'yes';

  // ===========================================================================
  // Add Student / Add Course dialogs (inline validation, dialog stays open on errors)
  // ===========================================================================

  function wireAddDialog({ dialog, form, errorEl, fields, openButton, submit }) {
    openButton.addEventListener('click', () => {
      form.reset();
      errorEl.textContent = '';
      dialog.showModal();
      fields.id.focus();
    });
    form.querySelector('[data-cancel]').addEventListener('click', () => dialog.close('cancel'));
    form.addEventListener('submit', (event) => {
      event.preventDefault();
      const result = submit();
      if (!result.ok) {
        errorEl.textContent = result.message;
        const target = fields[result.field] || fields.id;
        target.focus();
        target.select();
        return; // keep the dialog open
      }
      dialog.close('ok');
      refreshAll();
      setStatus(result.message);
    });
  }

  wireAddDialog({
    dialog: $('#dlg-student'), form: $('#form-student'), errorEl: $('#stu-error'),
    fields: { id: $('#stu-id'), name: $('#stu-name') }, openButton: $('#stu-add'),
    submit: () => system.addStudent($('#stu-id').value, $('#stu-name').value),
  });

  wireAddDialog({
    dialog: $('#dlg-course'), form: $('#form-course'), errorEl: $('#crs-error'),
    fields: { id: $('#crs-id'), name: $('#crs-name'), capacity: $('#crs-cap') }, openButton: $('#crs-add'),
    submit: () => {
      const cap = $('#crs-cap');
      // A number box reports "" for text such as "abc": hand a non-numeric text to the rules instead.
      const text = cap.validity.badInput ? 'not a number' : cap.value;
      return system.addCourse($('#crs-id').value, $('#crs-name').value, text);
    },
  });

  // ===========================================================================
  // Enroll / withdraw
  // ===========================================================================

  async function onEnroll() {
    const result = system.enroll($('#enr-student').value, $('#enr-course').value);
    if (!result.ok) {
      setStatus('Enrollment rejected.');
      await showError('Cannot enroll', result.message);
      return;
    }
    refreshAll();
    setStatus(result.message);
  }

  async function onWithdraw() {
    const studentId = $('#enr-student').value;
    const courseId = tblStudentCourses.selectedKey;
    if (!studentId || courseId === null) return;

    const student = system.findStudent(studentId);
    const course = system.findCourse(courseId);
    const question = student && course
      ? `Withdraw ${student.name} (${student.id}) from ${course.name} (${course.id})?`
      : 'Withdraw this student from the course?';
    if (!(await confirmYesNo('Confirm withdrawal', question))) return;

    const result = system.withdraw(studentId, courseId);
    if (!result.ok) {
      setStatus('Withdrawal rejected.');
      await showError('Cannot withdraw', result.message);
      return;
    }
    refreshAll();
    setStatus(result.message);
    $('#enr-student').focus(); // the Withdraw button just became disabled
  }

  $('#enr-student').addEventListener('change', () => { tblStudentCourses.selectedKey = null; refreshStudentCourses(); updateButtons(); });
  $('#enr-course').addEventListener('change', updateButtons);
  $('#enr-view-course').addEventListener('change', refreshCourseStudents);
  $('#btn-enroll').addEventListener('click', onEnroll);
  $('#btn-withdraw').addEventListener('click', onWithdraw);
  $('#stu-search').addEventListener('input', refreshStudents);
  $('#crs-search').addEventListener('input', refreshCourses);

  // ===========================================================================
  // Save / load / import / export
  // ===========================================================================

  function saveData() {
    const result = storage.saveToBrowser(system);
    if (!result.ok) {
      setStatus('Save failed.');
      showError('Cannot save', result.message);
      return false;
    }
    system.markSaved();
    updateTitle();
    setStatus(`Saved ${plural(system.students.length, 'student', 'students')}, ${plural(system.courses.length, 'course', 'courses')}, ` +
      `${plural(system.enrollments.length, 'enrollment', 'enrollments')} in this browser.`);
    return true;
  }

  /** Loads file texts into the system and reports problems in one warning. */
  async function applyFiles(files, { modified, origin }) {
    const report = storage.loadFromText(system, files);
    if (modified) system.modified = true; // differs from what is stored in the browser
    refreshAll();
    let status = `${origin}: ${plural(report.studentsLoaded, 'student', 'students')}, ` +
      `${plural(report.coursesLoaded, 'course', 'courses')}, ${plural(report.enrollmentsLoaded, 'enrollment', 'enrollments')}.`;
    if (report.warnings.length) status += ` Problems found: ${report.warnings.length}.`;
    setStatus(status);
    if (report.warnings.length)
      await showMessage({ title: 'Some data was skipped', text: storage.summarize(report, 15), kind: 'warning' });
  }

  function download(name, text) {
    const url = URL.createObjectURL(new Blob([text], { type: 'text/plain;charset=utf-8' }));
    const link = h('a', { href: url, download: name });
    document.body.append(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
  }

  function exportFiles() {
    const files = storage.serialize(system);
    ['students', 'courses', 'enrollments'].forEach((name, i) =>
      setTimeout(() => download(`${name}.txt`, files[name]), i * 250));
    setStatus('Exported students.txt, courses.txt and enrollments.txt (check your Downloads folder).');
  }

  async function importFiles(fileList) {
    const files = {};
    for (const file of fileList) {
      const name = file.name.toLowerCase();
      for (const kind of ['students', 'courses', 'enrollments']) {
        if (name.includes(kind) && name.endsWith('.txt')) files[kind] = await file.text();
      }
    }
    const found = Object.keys(files);
    if (found.length === 0) {
      await showError('Nothing to import', 'Choose students.txt, courses.txt and/or enrollments.txt.');
      return;
    }
    const missing = ['students', 'courses', 'enrollments'].filter((k) => !(k in files)).map((k) => `${k}.txt`);
    const note = missing.length ? `\n\nNot selected (treated as empty): ${missing.join(', ')}.` : '';
    if (!(await confirmYesNo('Import files', `Replace ALL current data with the selected files?${note}`))) return;
    await applyFiles(files, { modified: true, origin: 'Imported' });
  }

  $('#btn-save').addEventListener('click', saveData);
  $('#btn-export').addEventListener('click', exportFiles);
  $('#btn-import').addEventListener('click', () => $('#file-input').click());
  $('#file-input').addEventListener('change', (event) => {
    const chosen = [...event.target.files];
    event.target.value = ''; // allows choosing the same files again
    if (chosen.length) importFiles(chosen);
  });
  $('#btn-sample').addEventListener('click', async () => {
    if (await confirmYesNo('Sample data', 'Replace ALL current data with the demo data (6 students, 4 courses, 12 enrollments)?'))
      await applyFiles(storage.SAMPLE, { modified: true, origin: 'Sample data' });
  });
  $('#btn-about').addEventListener('click', () => $('#dlg-about').showModal());

  document.addEventListener('keydown', (event) => {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 's') {
      event.preventDefault();
      saveData();
    }
  });

  // The browser can only offer its standard "leave this page?" prompt (no Yes/No/Cancel).
  window.addEventListener('beforeunload', (event) => {
    if (system.modified) { event.preventDefault(); event.returnValue = ''; }
  });

  // ===========================================================================
  // Tabs
  // ===========================================================================

  const TAB_NAMES = ['students', 'courses', 'enrollment'];

  function selectTab(name, moveFocus) {
    for (const tabName of TAB_NAMES) {
      const tab = $(`#tab-${tabName}`);
      const selected = tabName === name;
      tab.setAttribute('aria-selected', String(selected));
      tab.tabIndex = selected ? 0 : -1;
      $(`#panel-${tabName}`).hidden = !selected;
      if (selected && moveFocus) tab.focus();
    }
  }

  TAB_NAMES.forEach((name, index) => {
    const tab = $(`#tab-${name}`);
    tab.addEventListener('click', () => selectTab(name, false));
    tab.addEventListener('keydown', (event) => {
      const step = { ArrowRight: 1, ArrowLeft: -1 }[event.key];
      let target = null;
      if (step) target = TAB_NAMES[(index + step + TAB_NAMES.length) % TAB_NAMES.length];
      else if (event.key === 'Home') target = TAB_NAMES[0];
      else if (event.key === 'End') target = TAB_NAMES[TAB_NAMES.length - 1];
      if (target) { event.preventDefault(); selectTab(target, true); }
    });
  });

  // ===========================================================================
  // Start-up: load what this browser saved, or the demo data on the first visit
  // ===========================================================================

  const saved = storage.readFromBrowser();
  if (saved) {
    applyFiles(saved, { modified: false, origin: 'Loaded' });
  } else {
    applyFiles(storage.SAMPLE, { modified: false, origin: 'Welcome! Demo data loaded' })
      .then(() => setStatus(lastAction + ' Press Save to keep your changes in this browser.'));
  }
})();
