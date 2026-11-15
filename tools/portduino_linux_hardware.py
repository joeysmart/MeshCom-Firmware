from pathlib import Path

Import("env")

if env["PIOENV"] == "raspberry_pi":
    framework_dir = Path(env.PioPlatform().get_package_dir("framework-portduino"))
    simulated_dir = framework_dir / "cores" / "portduino" / "simulated"
    for source in simulated_dir.glob("*.cpp"):
        contents = source.read_text()
        if not contents.startswith("#ifndef PORTDUINO_LINUX_HARDWARE"):
            source.write_text(
                "#ifndef PORTDUINO_LINUX_HARDWARE\n"
                + contents
                + "\n#endif // PORTDUINO_LINUX_HARDWARE\n"
            )
