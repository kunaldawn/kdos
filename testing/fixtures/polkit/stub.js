/*
 * polkit's `polkit` object, stubbed, so a .rules file can be evaluated off the
 * system by the same engine polkit uses.
 *
 * A .rules file is JavaScript run by duktape. A syntax error or a rule that
 * grants the wrong thing is caught by no compiler and shows up as networking
 * that quietly does not work, so it is worth running.
 */
var polkit = {
    Result: { YES: "yes", NO: "no", AUTH_ADMIN: "auth_admin",
              AUTH_ADMIN_KEEP: "auth_admin_keep", NOT_HANDLED: undefined },
    addRule: function (f) { polkit._rules.push(f); },
    addAdminRule: function (f) { polkit._admin.push(f); },
    _rules: [],
    _admin: [],
    log: function () {}
};

/* polkit runs the registered rules in order and stops at the first that
 * returns something. Modelled, so a file that adds two rules is evaluated the
 * way polkit would evaluate it. */
polkit._ask = function (id, subject) {
    for (var i = 0; i < polkit._rules.length; i++) {
        var r = polkit._rules[i]({ id: id }, subject);
        if (r !== undefined)
            return r;
    }
    return undefined;
};
