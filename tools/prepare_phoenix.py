"""Apply two reviewed compiler fixes to the pinned public Phoenix headers.

The submodule commit stays fixed. identity() records the patched source digest,
so an unpatched kernel cache cannot be mistaken for this build's dependency.
"""
from pathlib import Path


def prepare(root):
    root = Path(root)
    fixes = [
        ('src/native/model/weight_source.h',
         'actual.QuadPart != bytes',
         '(actual.QuadPart < 0 || static_cast<unsigned long long>(actual.QuadPart) != bytes)'),
        ('src/native/model/bundle_loader.h',
         '''for (const auto& source : launches) {
                    require(source.at("index") == index, "launch index");
                    if (index > 0 && index < 155) {
                        const auto& spec = kernels.at(source.at("name").get<std::string>());
                        require(spec.at("parameter_bytes") == source.at("argument_bytes") &&
                                    spec.at("block") == source.at("block"),''',
         '''for (const auto& launch : launches) {
                    require(launch.at("index") == index, "launch index");
                    if (index > 0 && index < 155) {
                        const auto& spec = kernels.at(launch.at("name").get<std::string>());
                        require(spec.at("parameter_bytes") == launch.at("argument_bytes") &&
                                    spec.at("block") == launch.at("block"),'''),
    ]
    for relative, before, after in fixes:
        path = root / relative
        source = path.read_text(encoding='utf-8')
        if after in source:
            continue
        if source.count(before) != 1:
            raise RuntimeError('Pinned Phoenix header changed: ' + relative)
        path.write_text(source.replace(before, after), encoding='utf-8')
