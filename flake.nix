{
  description = "Loom development environment";

  inputs.nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0";

  outputs = { self, nixpkgs }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
  in {
    devShells.${system}.default = pkgs.mkShell {
      packages = with pkgs; [
        llvmPackages_20.libcxxClang
        cmake
        fmt_9
        glfw
        fast-float
        range-v3
        graphviz                # docs
        doxygen                 # docs
      ];

      shellHook = ''
      export CMAKE_EXPORT_COMPILE_COMMANDS=1
      export NIX_ENFORCE_NO_NATIVE=0
      '';
    };
  };
}
