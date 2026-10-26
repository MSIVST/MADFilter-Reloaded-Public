"""Exercise packaged batch entry points without registration or a game launch.

Run with Python 3 on Windows. The .NET Framework C# compiler builds a disposable
helper stand-in; no actual MADFilter binary is invoked. Output stays in local/.
"""
import base64
import json
import os
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[3]
SCRIPTS = ROOT / 'MADFilter-Reloaded/packaging/bat'
WORK = ROOT / "local/batch-helper-tests/four-scripts/space & apostrophe's ! folder"
WORK.mkdir(parents=True, exist_ok=True)
for script in SCRIPTS.glob('*.bat'):
    shutil.copy2(script, WORK / script.name)

source = WORK / 'HelperStub.cs'
source.write_text('''using System;
using System.IO;
using System.Text;
class HelperStub {
    static int Main(string[] args) {
        using (var writer = new StreamWriter(Environment.GetEnvironmentVariable("BATCH_TEST_LOG"))) {
            foreach (var arg in args)
                writer.WriteLine(Convert.ToBase64String(Encoding.UTF8.GetBytes(arg)));
            writer.WriteLine("POLICY=" + Environment.GetEnvironmentVariable("MADFILTER_BSAUDIO"));
        }
        return Int32.Parse(Environment.GetEnvironmentVariable("BATCH_TEST_EXIT"));
    }
}
''', encoding='utf-8')
compiler = Path(os.environ['WINDIR']) / 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
helper = WORK / 'MADFilterRegister.exe'
subprocess.run([str(compiler), '/nologo', '/target:exe', '/out:' + str(helper), str(source)], check=True)
game = WORK / "Game & test's ! name.exe"
game.write_bytes(b'test fixture; never executed')
log = WORK / 'arguments.txt'
results = []


def run(name, arguments, expected, returncode=0, input_text='', policy=''):
    if log.exists():
        log.unlink()
    env = os.environ.copy()
    env.update(BATCH_TEST_LOG=str(log), BATCH_TEST_EXIT=str(returncode))
    env.pop('MADFILTER_BSAUDIO', None)
    # /d disables user AutoRun, /v:off protects literal exclamation marks.
    command = '"' + str(WORK / name) + '" ' + arguments
    invocation = 'cmd.exe /d /v:off /s /c "' + command + '"'
    result = subprocess.run(invocation, input=input_text, text=True, capture_output=True,
                            env=env, cwd=ROOT, timeout=15)
    assert result.returncode == returncode, (name, result.returncode, result.stdout, result.stderr)
    if expected is None:
        assert not log.exists(), (name, 'helper must not run')
    else:
        lines = log.read_text(encoding='utf-8-sig').splitlines()
        actual = [base64.b64decode(line).decode('utf-8') for line in lines[:-1]]
        assert actual == expected, (name, actual, expected)
        assert lines[-1] == 'POLICY=' + policy, (name, lines[-1])
    results.append({'script': name, 'arguments': arguments, 'exit': returncode, 'result': 'PASS'})


commands = {'Register.bat': '/fallout3', 'Unregister.bat': '/unregister',
            'Status.bat': '/status'}
assert {p.name for p in SCRIPTS.glob('*.bat')} == {*commands, 'Run-Game.bat'}
for name, command in commands.items():
    run(name, '/nopause', [command])
    run(name, '/nopause', [command], returncode=31)
for name, policy in [('Run-Game.bat', '')]:
    for arguments in ['/nopause "' + str(game) + '"', '"' + str(game) + '" /nopause']:
        run(name, arguments, ['/launch', str(game)], policy=policy)
    run(name, '/nopause', ['/launch', str(game)], input_text='"' + str(game) + '"\n', policy=policy)
    run(name, '/nopause "' + str(game) + '"', ['/launch', str(game)], returncode=31, policy=policy)
    run(name, '/nopause "missing-game.exe"', None, returncode=1)
    run(name, '/nopause', None, returncode=1, input_text='\n')

# The previous status wrapper could return success with a missing helper.
helper.replace(WORK / 'helper-disabled.exe')
for name in [*commands, 'Run-Game.bat']:
    run(name, '/nopause', None, returncode=1)
report = ROOT / 'local/batch-helper-tests/results.json'
report.write_text(json.dumps({'cases': len(results), 'results': results}, indent=2), encoding='utf-8')
print(f'{len(results)} batch-helper cases passed. No registration or game launch performed.')
