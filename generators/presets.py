"""Writes definition.json's parameters and presets, and presets/<name>.json, from
the table below: the one place the factory sounds are defined. Run from the
repository root after changing it:
    python generators/presets.py .
The control defaults are the machine's plain init sound; the builder makes the
init preset (default.json) from them, so the twelve factory presets keep their names."""
import json, sys, copy, os

root = sys.argv[1] if len(sys.argv) > 1 else '.'

# id, label, default, mapping (how the C++ reads it), display
CONTROLS = [
    ('stops', 'Stops', 0.3, {'kind': 'linear', 'minimum': 0, 'maximum': 1}, 'percent'),
    ('body', 'Body', 0.5, {'kind': 'linear', 'minimum': 0, 'maximum': 1}, 'percent'),
    ('bite', 'Bite', 0.3, {'kind': 'linear', 'minimum': 0, 'maximum': 1}, 'percent'),
    ('drift', 'Drift', 0.0, {'kind': 'linear', 'minimum': 0, 'maximum': 1}, 'percent'),
    ('attack', 'Attack', 0.15, {'kind': 'exponential', 'minimum': 0.003, 'maximum': 1.5}, 'duration'),
    ('reverb', 'Reverb', 0.3, {'kind': 'linear', 'minimum': 0, 'maximum': 1}, 'percent'),
    ('delay', 'Delay', 0.0, {'kind': 'linear', 'minimum': 0, 'maximum': 1}, 'percent'),
    ('feedback', 'Feedback', 0.3, {'kind': 'linear', 'minimum': 0, 'maximum': 0.85}, 'percent'),
]

# name, file, stops, body, bite, drift, attack, reverb, delay, feedback, amp release (stock units)
PRESETS = [
    ('Vespers',      'vespers',      0.22, 0.55, 0.25, 0.12, 0.18, 0.35, 0.00, 0.20, 12000),
    ('Devils March', 'devils-march', 0.55, 0.95, 0.45, 0.15, 0.05, 0.30, 0.25, 0.30, 9000),
    ('Ashen',        'ashen',        0.15, 0.10, 0.12, 0.20, 0.35, 0.85, 0.00, 0.00, 22000),
    ('Black Cantor', 'black-cantor', 0.40, 0.30, 0.90, 0.18, 0.08, 0.40, 0.35, 0.40, 11000),
    ('Reliquary',    'reliquary',    0.10, 0.80, 0.15, 0.00, 0.25, 0.45, 0.00, 0.20, 14000),
    ('Unlit',        'unlit',        0.20, 0.40, 0.15, 0.20, 0.70, 0.85, 0.10, 0.30, 20000),
    ('Adversary',    'adversary',    0.85, 0.40, 0.75, 0.70, 0.10, 0.45, 0.30, 0.55, 12000),
    ('Midnight',     'midnight',     0.45, 0.60, 0.40, 0.15, 0.10, 0.40, 0.40, 0.35, 10000),
    ('Endless',      'endless',      1.00, 0.70, 0.50, 0.30, 0.45, 0.95, 0.15, 0.25, 26000),
    ('Hollow Saint', 'hollow-saint', 0.60, 0.05, 0.35, 0.20, 0.30, 0.55, 0.10, 0.30, 16000),
    ('Last Office',  'last-office',  0.30, 1.00, 0.05, 0.30, 1.00, 0.50, 0.35, 0.70, 22000),
    ('Diabolus',     'diabolus',     0.80, 1.00, 0.60, 0.45, 0.12, 0.60, 0.35, 0.50, 14000),
]

# A version 4 OP-XY preset as the device saves one (the same template as
# Daath's); engine params and the amp envelope are filled in below.
TEMPLATE = json.loads('{"engine": {"bendrange": 8191, "highpass": 0, "modulation": {"aftertouch": {"amount": 16384, "target": 0}, "modwheel": {"amount": 16384, "target": 0}, "pitchbend": {"amount": 16384, "target": 0}, "velocity": {"amount": 16384, "target": 0}}, "params": [0, 0, 0, 0, 0, 0, 0, 0], "playmode": "poly", "portamento.amount": 0, "portamento.type": 0, "transpose": 0, "tuning.root": 0, "tuning.scale": 0, "velocity.sensitivity": 0, "volume": 16384, "width": 0}, "envelope": {"amp": {"attack": 0, "decay": 8192, "release": 6000, "sustain": 32767}, "filter": {"attack": 0, "decay": 0, "release": 0, "sustain": 32767}}, "fx": {"active": false, "params": [0, 0, 0, 0, 0, 32767, 0, 0], "type": "svf"}, "lfo": {"active": false, "params": [0, 0, 0, 0, 0, 0, 0, 0], "type": "value"}, "octave": 0, "platform": "OP-XY", "regions": [], "type": "diabolus", "version": 4}')


# Presets that switch on the OP-XY's own Tremolo LFO, an organ's tremulant.
# It uses the stock Tremolo's own default settings, read from the stock
# instrument catalog (profiles/opxy-1.2.1/content/op1_factory.json in the SDK,
# "lfo_types"), so nothing about the stock LFO format is guessed. The engine's
# controls stay free for the Value, Random and Element LFOs.
TREMULANT = {'Black Cantor', 'Unlit', 'Endless', 'Last Office'}
STOCK_TREMOLO = [14848, 16384, 19000, 16384, 0, 0, 0, 0]


def main():
    path = os.path.join(root, 'definition.json')
    d = json.load(open(path))
    parameters = []
    for i, (cid, label, default, mapping, display) in enumerate(CONTROLS):
        p = {'id': cid, 'label': label}
        if i >= 4:
            p['control'] = i - 4
        p.update({'default': default, 'mapping': mapping, 'display': display})
        parameters.append(p)
    d['parameters'] = parameters
    d['presets'] = {name: {c[0]: v for c, v in zip(CONTROLS, values)} for name, _, *values, _ in PRESETS}
    with open(path, 'w') as f:
        json.dump(d, f, indent=2)
        f.write('\n')
    folder = os.path.join(root, 'presets')
    os.makedirs(folder, exist_ok=True)
    for name in os.listdir(folder):
        if name.endswith('.json'):
            os.remove(os.path.join(folder, name))
    for name, file, *values, release in PRESETS:
        p = copy.deepcopy(TEMPLATE)
        p['type'] = d['id']
        p['engine']['params'] = [round(v * 32767) for v in values]
        p['envelope']['amp'] = {'attack': 0, 'decay': 8192, 'release': release, 'sustain': 32767}
        if name in TREMULANT:
            p['lfo'] = {'active': True, 'params': list(STOCK_TREMOLO), 'type': 'tremolo'}
        with open(os.path.join(folder, file + '.json'), 'w') as f:
            json.dump(p, f, indent=2, sort_keys=True)
            f.write('\n')

    # The same table for the host tests, so they always check what ships.
    with open(os.path.join(root, 'tests', 'presets.h'), 'w') as f:
        f.write('// Generated by generators/presets.py from its PRESETS table: do not edit.\n#pragma once\n')
        f.write('struct Preset { const char* name; const char* file; double values[8]; int release; };\n')
        f.write('static const unsigned preset_count = %d;\n' % len(PRESETS))
        f.write('static const double control_defaults[8] = {%s};\n' % ', '.join(repr(c[2]) for c in CONTROLS))
        f.write('static const Preset presets[preset_count] = {\n')
        for name, file, *values, release in PRESETS:
            f.write('    {"%s", "%s", {%s}, %d},\n' % (name, file, ', '.join(repr(v) for v in values), release))
        f.write('};\n')


main()
