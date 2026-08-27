// Stand-in translation unit for a board profile with hasAgent=false (#263).
//
// The microfi_agent component still exists in the build -- its Kconfig.projbuild
// is what defines CONFIG_MICROFI_WIFI_SSID/PASSWORD, which main.cpp's WiFi
// pre-provision reads on every board, agent or not -- but none of the MicroFi
// sources are compiled and no agent task is ever created. main.cpp does not
// call microfi_agent_start() on such a board (it never includes the header),
// so nothing here needs to exist beyond giving the library one object file.
namespace microfi_agent_stub {
constexpr bool kAgentDisabledByBoardProfile = true;
}
