/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   groups.js — the Make It Yours page's rules, apart from its controls
 *
 *   rows(value)          the groups `kdos update groups --json` printed, each
 *                        {id, title, installed, missing, packages, size};
 *                        size is null where the command could not size it.
 *                        An entry whose id is not a port-name word is
 *                        dropped: the id becomes an argument of a root
 *                        command
 *   noIndex(list)        no group has a size: the release's binhost index
 *                        is not on this machine, and nothing installs
 *                        prebuilt until it is
 *   installable(g)       not installed, and sized, so every package of it
 *                        is prebuilt
 *   bytes(list, ids)     what the ticked installable groups download
 *   start(ids)           the install queue, one group at a time
 *   step(state, code, text)
 *                        the queue after the current group's helper exited
 *                        with `code`: done or failed (with its text), and
 *                        the next one current; current is "" at the end
 * ---------------------------------
 */

.pragma library

const ID = /^[A-Za-z0-9][A-Za-z0-9+._-]*$/;

function rows(value) {
    if (!Array.isArray(value)) {
        return [];
    }
    return value.filter(g => g !== null && typeof g === "object" && typeof g.id === "string" && ID.test(g.id))
        .map(g => ({
            id: g.id,
            title: typeof g.title === "string" && g.title.length > 0 ? g.title : g.id,
            installed: g.installed === true,
            missing: typeof g.missing === "number" ? g.missing : 0,
            packages: typeof g.packages === "number" ? g.packages : 0,
            size: typeof g.size === "number" ? g.size : null,
        }));
}

function noIndex(list) {
    return list.length > 0 && list.every(g => g.size === null);
}

function installable(g) {
    return !g.installed && g.size !== null;
}

function bytes(list, ids) {
    return list.filter(g => ids.indexOf(g.id) >= 0 && installable(g))
        .reduce((sum, g) => sum + g.size, 0);
}

function start(ids) {
    return { queue: ids.slice(), total: ids.length, done: [], failed: [], current: ids.length > 0 ? ids[0] : "" };
}

function step(state, code, text) {
    const queue = state.queue.slice(1);
    return {
        queue: queue,
        total: state.total,
        done: code === 0 ? state.done.concat([state.current]) : state.done,
        failed: code === 0 ? state.failed : state.failed.concat([{ id: state.current, text: text }]),
        current: queue.length > 0 ? queue[0] : "",
    };
}
