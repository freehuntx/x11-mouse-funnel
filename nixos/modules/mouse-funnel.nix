{
  lib,
  pkgs,
  config,
  ...
}:
let
  cfg = config.services.mouse-funnel;
in
{
  options.services.mouse-funnel = {
    enable = lib.mkEnableOption "mouse-funnel systemd user service";
    package = lib.mkOption {
      type = lib.types.package;
      description = "The mouse-funnel package to use.";
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ cfg.package ];

    systemd.user.services.mouse-funnel = {
      description = "X11 Mouse Funnel";
      partOf = ["graphical-session.target"];
      wantedBy = ["default.target"];
      after = ["graphical-session-pre.target"];

      serviceConfig = {
        ExecStart = "${cfg.package}/bin/mouse_funnel";
        Restart = "on-failure";
        RestartSec = 1;
      };
    };
  };
}
