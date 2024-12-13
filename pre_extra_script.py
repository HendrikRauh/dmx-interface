Import("env")

if env.IsIntegrationDump():
    # stop the current script execution
    Return()

env.Execute("git submodule update --init --recursive")
print("✅ SUBMODULES UPDATED")
