// A settings form drawn from the schema (ADR-008: no field written into the page by hand). A resource's fields in its
// groups, each a control by its type: a switch, a choice (segmented when it has few short options, a list
// otherwise), a number with its unit, a line of text, a colour. A change is written at once (PATCH) and is live; the
// page shows whether the resource is stored, and storing is one action (web-ui-design, rule 7). A refusal is said
// under its field, in the unit's words. A heading says only what nothing above it says (web-ui-design, rule 16).

const SCHEMA = {resources: null, promise: null};

async function schemaResources() {
    if (SCHEMA.resources === null) {
        SCHEMA.promise = SCHEMA.promise || api("GET", "/api/v1/schema");
        const reply = await SCHEMA.promise;
        SCHEMA.promise = null;
        if (!reply.ok) {
            return [];
        }
        SCHEMA.resources = reply.data.resources;
    }
    return SCHEMA.resources;
}

async function resourceOf(path) {
    const template = path.replace(/\/\d+$/, "/{id}");
    return (await schemaResources()).find((resource) => resource.path === template) || null;
}

// Choices left out of the page by decision: the simulated scale, a debug feature (the owner, ADR-010)
const LEFT_OUT = {driver: ["simulated"]};

function choicesOf(field) {
    return (field.choices || []).filter((choice) => !(LEFT_OUT[field.name] || []).includes(choice.value));
}

function decimalsOf(field) {
    const step = field.step || 0;
    return field.type === "integer" ? 0 : (step > 0 && step < 1 ? Math.min(String(step).split(".")[1].length, 4) : 3);
}

// features: 40 47 50 51 54
function control(id, field, value, write) {
    if (field.type === "boolean") {
        const input = element("input", {type: "checkbox", id, checked: value === true});
        input.onchange = () => write(input.checked);
        return element("label", {class: "switch"}, input, element("span"));
    }
    if (field.type === "choice") {
        const choices = choicesOf(field);
        const short = choices.every((choice) => (choice.short_label || choice.label).length <= 12);
        if (choices.length <= 4 && short) {
            const group = element("div", {class: "segmented", role: "radiogroup", id});
            for (const choice of choices) {
                const button = element("button", {type: "button", role: "radio", "aria-checked": String(choice.value === value),
                                                  class: choice.value === value ? "on" : ""},
                                       choice.short_label || choice.label);
                button.onclick = () => write(choice.value);
                group.append(button);
            }
            return group;
        }
        const select = element("select", {id}, ...choices.map((choice) => {
            const option = new Option(choice.label, String(choice.value));
            option.selected = choice.value === value;
            return option;
        }));
        select.onchange = () => {
            const chosen = choices.find((choice) => String(choice.value) === select.value);
            write(chosen.value);
        };
        return select;
    }
    if (field.type === "colour") {
        const input = element("input", {type: "color", id, value: value || "#000000"});
        input.onchange = () => write(input.value);
        return element("div", {class: "colour"}, input, element("code", {}, value || ""));
    }
    if (field.type === "number" || field.type === "integer") {
        const shown = value === null || value === undefined ? "" : String(parseFloat(Number(value).toFixed(decimalsOf(field))));
        const input = element("input", {type: "text", id, value: shown, inputmode: field.type === "integer" ? "numeric" : "decimal",
                                        placeholder: field.nullable ? "Not set" : ""});
        input.onchange = () => {
            const text = input.value.trim();
            write(text === "" && field.nullable ? null : Number(text));
        };
        return element("div", {class: "number"}, input, unitOf(field) ? element("span", {class: "unit-of"}, unitOf(field)) : null);
    }
    // A text setting is a name or an address taken as typed (a network, the update's source): a phone neither
    // capitalises nor corrects it
    const input = element("input", {type: field.access === "write_only" ? "password" : "text", id,
                                    value: field.access === "write_only" ? "" : (value || ""),
                                    maxlength: field.max_length || false, autocomplete: "off",
                                    autocapitalize: "none", autocorrect: "off", spellcheck: "false",
                                    placeholder: field.access === "write_only" ? "Unchanged" : ""});
    input.onchange = () => write(input.value);
    return input;
}

// A control's id, unique on the page: two forms of one kind may be on it at once (the motors' Coarse and Fine)
function fieldId(path, field) {
    return `f-${path.split("/").slice(3).join("-")}-${field.name}`;
}

// The schema's unit "weight" is the scale's own unit (grains or grams): no word of its own beside a field; a flow,
// "weight/s", is "/s" beside it
function unitOf(field) {
    return (field.unit || "").replace("weight", "");
}

function limitsOf(field) {
    const parts = [];
    if (typeof field.min === "number" && typeof field.max === "number") {
        const unit = field.unit === "weight" ? " in the scale's unit"
            : field.unit === "weight/s" ? " in the scale's unit a second" : (unitOf(field) ? " " + unitOf(field) : "");
        parts.push(`${field.min} to ${field.max}${unit}`);
    }
    // When it takes effect, in the schema's words, unless at once (features: 58)
    if (field.applies === "at_restart") {
        parts.push("takes effect at restart");
    } else if (field.applies && field.applies !== "now") {
        parts.push(`takes effect ${field.applies}`);
    }
    const said = parts.join("; ");
    return said.charAt(0).toUpperCase() + said.slice(1);     // a sentence of its own after the field's line
}

// The form for one resource at one path: its groups and fields, live, and its store bar. `hooks.changed(values, path)` after
// every write, so what else the page shows of the resource (a list's "Not stored", a table's figures) follows at once;
// `hooks.reload()` after a store or an undo, which change what every resource of the slot holds. `hooks.choices`, by
// a field's name: the choices of a field that is an id of something the unit lists (a load's powder), drawn by name
// features: 41 42 43 44 52
async function drawForm(path, values, hooks) {
    const resource = await resourceOf(path);
    if (resource === null) {
        return element("p", {class: "hint"}, "This unit does not describe these settings.");
    }
    const form = element("div", {class: "form"});
    // Changed, not stored: put back what is stored, or store it. Undo is offered where the unit has its revert
    // (ADR-008, gate 2; the stand-in has it now)
    const undo = resource.revert ? element("button", {class: "undo", type: "button"}, "Undo changes") : null;
    const store = element("button", {class: "action go small", type: "button"}, "Store");
    const storeBar = element("div", {class: "store-bar", hidden: values.saved !== false},
                             element("span", {}, "Changed, not stored: lost at the next restart."),
                             element("div", {class: "store-actions"}, undo, store));
    const finish = async (to, done, refused) => {
        const reply = await api("POST", to, {});
        if (!reply.ok) {
            refusal(reply, refused);
            return;
        }
        hooks.reload();      // drawn again first: a page drawn clears the message line
        say(done, "good");
    };
    store.onclick = () => finish(resource.save, "Stored.", "Not stored");
    if (undo) {
        undo.onclick = () => finish(resource.revert, "Changes undone: as stored.", "Not undone");
    }
    const groups = new Map();
    const choices = hooks.choices || {};
    const fields = resource.fields.map((field) => choices[field.name]
        ? Object.assign({}, field, {type: "choice", choices: choices[field.name], min: undefined, max: undefined})
        : field);
    for (const field of fields) {
        if (!groups.has(field.group)) {
            groups.set(field.group, []);
        }
        groups.get(field.group).push(field);
    }
    for (const [name, fields] of groups) {
        // A field with no group is the resource's own: its heading would repeat the part's name, so it has none
        // A group with a heading of its own, not a fieldset: WebKit draws a fieldset's legend into its border, however styled
        const titleId = name ? `g-${path.split("/").slice(3).join("-")}-${name.replace(/\W+/g, "-")}` : null;
        const set = element("div", {class: "fields", role: "group", "aria-labelledby": titleId},
                            name ? element("h3", {class: "fields-title", id: titleId}, name) : null);
        for (const field of fields) {
            let row = null;
            const error = element("p", {class: "field-error", hidden: true});
            const write = async (value) => {
                const reply = await api("PATCH", path, {[field.name]: value});
                if (!reply.ok) {
                    error.textContent = refusalText(reply);
                    error.hidden = false;
                    row.replaceWith(redraw(values[field.name]));
                    return;
                }
                Object.assign(values, reply.data);
                error.hidden = true;
                storeBar.hidden = values.saved !== false;
                hooks.changed(values, path);
                row.replaceWith(redraw(values[field.name]));
            };
            const redraw = (value) => {
                const fresh = element("div", {class: field.type === "boolean" ? "field inline" : "field"},
                                      element("label", {for: fieldId(path, field)}, field.label),
                                      control(fieldId(path, field), field, value, write),
                                      element("p", {class: "field-help"}, [field.about, limitsOf(field)].filter(Boolean).join(". ")),
                                      error);
                row = fresh;
                return fresh;
            };
            set.append(redraw(values[field.name]));
        }
        form.append(set);
    }
    form.append(storeBar);
    return form;
}
