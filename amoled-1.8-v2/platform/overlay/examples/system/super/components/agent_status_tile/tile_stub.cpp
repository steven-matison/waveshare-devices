// Stand-in translation unit for a board profile with hasAgent=false (#263).
// No IAppProvider is registered, so no "microfi.agent.status" app exists on
// the board and there is no -u link flag keeping a registrar alive.
namespace agent_status_tile_stub {
constexpr bool kTileDisabledByBoardProfile = true;
}
