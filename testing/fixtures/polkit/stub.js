/*
 * polkit's `polkit` object, stubbed, so a .rules file can be evaluated off the
 * system by the same engine polkit uses, and the answer polkitd would give
 * worked out from the rules and the actions' shipped defaults together.
 *
 * A .rules file is JavaScript run by duktape. A syntax error or a rule that
 * grants the wrong thing is caught by no compiler and shows up as a setting
 * that quietly does not change, or a password nobody should have been asked
 * for, so it is worth running.
 */
var polkit = {
    Result: { YES: "yes", NO: "no", AUTH_SELF: "auth_self",
              AUTH_SELF_KEEP: "auth_self_keep", AUTH_ADMIN: "auth_admin",
              AUTH_ADMIN_KEEP: "auth_admin_keep", NOT_HANDLED: undefined },
    addRule: function (f) { polkit._rules.push(f); },
    addAdminRule: function (f) { polkit._admin.push(f); },
    _rules: [],
    _admin: [],
    /* [allow_any, allow_inactive, allow_active] per action id, from
     * defaults.js; an element the .policy file omits is "no", as polkitd
     * reads it. */
    _defaults: {},
    /* The same, read from the .policy files KDOS ships and, when one has been
     * built, from the tree's own, for the check that the model agrees. */
    _shipped: {},
    _built: {},
    log: function () {}
};

/*
 * A subject as polkitd hands it to a rule: its groups, and whether its
 * session is local and active. `who` names it in a failure.
 */
polkit._subject = function (who, groups, local, active) {
    return {
        who: who,
        user: who,
        groups: groups,
        local: local,
        active: active,
        isInGroup: function (g) {
            for (var i = 0; i < groups.length; i++)
                if (groups[i] === g)
                    return true;
            return false;
        },
        isInNetGroup: function () { return false; }
    };
};

/* polkitd runs the registered rules in order and stops at the first that
 * returns something. Modelled, so a file that adds two rules is evaluated the
 * way polkit would evaluate it. undefined: no rule answered. */
polkit._ask = function (id, subject) {
    var action = { id: id, lookup: function () { return undefined; } };

    for (var i = 0; i < polkit._rules.length; i++) {
        var r = polkit._rules[i](action, subject);
        if (r !== undefined)
            return r;
    }
    return undefined;
};

/* The whole decision: a rule's answer, or else the default for the
 * subject's session — allow_active for an active local session,
 * allow_inactive for an inactive local one, allow_any for everything else.
 * null when the action is not modelled at all. */
polkit._decide = function (id, subject) {
    var r = polkit._ask(id, subject);
    var d = polkit._defaults[id];

    if (r !== undefined)
        return r;
    if (!d)
        return null;
    if (subject.local && subject.active)
        return d[2];
    if (subject.local)
        return d[1];
    return d[0];
};
