// A settings form drawn from the schema (ADR-008: no field written into the page by hand). A resource's fields in its
// groups, each a control by its type: a switch, a choice (segmented when it has few short options, a list
// otherwise), a number with its unit, a line of text, a colour. A change is written at once (PATCH) and is live; the
// page shows whether the resource is stored, and storing is one action (web-ui-design, rule 7). A refusal is said
// under its field, in the unit's words.

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

function control(field, value, write) {
    const id = `f-${field.name}`;
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
    const input = element("input", {type: field.access === "write_only" ? "password" : "text", id,
                                    value: field.access === "write_only" ? "" : (value || ""),
                                    maxlength: field.max_length || false, autocomplete: "off",
                                    placeholder: field.access === "write_only" ? "Unchanged" : ""});
    input.onchange = () => write(input.value);
    return input;
}

// The schema's unit "weight" is the scale's own unit (grains or grams): no word of its own beside a field
function unitOf(field) {
    return field.unit === "weight" ? "" : (field.unit || "");
}

function limitsOf(field) {
    const parts = [];
    if (typeof field.min === "number" && typeof field.max === "number") {
        const unit = field.unit === "weight" ? " in the scale's unit" : (unitOf(field) ? " " + unitOf(field) : "");
        parts.push(`${field.min} to ${field.max}${unit}`);
    }
    if (field.applies === "at_restart") {
        parts.push("takes effect at restart");
    }
    return parts.join("; ");
}

// The form for one resource at one path: its groups and fields, live, and its store bar
async function drawForm(path, values, onStored) {
    const resource = await resourceOf(path);
    if (resource === null) {
        return element("p", {class: "hint"}, "This unit does not describe these settings.");
    }
    const form = element("div", {class: "form"});
    const storeBar = element("div", {class: "store-bar", hidden: values.saved !== false},
                             element("span", {}, "Changed, not stored: lost at the next restart."),
                             element("button", {class: "action go small", type: "button"}, "Store"));
    storeBar.querySelector("button").onclick = async () => {
        const reply = await api("POST", resource.save, {});
        if (!reply.ok) {
            refusal(reply, "Not stored");
            return;
        }
        storeBar.hidden = true;
        say("Stored.", "good");
        if (onStored) {
            onStored();
        }
    };
    const groups = new Map();
    for (const field of resource.fields) {
        if (!groups.has(field.group)) {
            groups.set(field.group, []);
        }
        groups.get(field.group).push(field);
    }
    for (const [name, fields] of groups) {
        const set = element("fieldset", {class: "fields"}, element("legend", {}, name));
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
                row.replaceWith(redraw(values[field.name]));
            };
            const redraw = (value) => {
                const fresh = element("div", {class: field.type === "boolean" ? "field inline" : "field"},
                                      element("label", {for: `f-${field.name}`}, field.label),
                                      control(field, value, write),
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
