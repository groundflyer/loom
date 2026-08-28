{
  description = "Loom development environment";

  inputs.nixpkgs.url = "github:/NixOS/nixpkgs/nixos-25.11";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      llvm = pkgs.llvmPackages;
      clangStdenv = pkgs.overrideCC pkgs.stdenv llvm.libcxxClang;
      fmtLibcxx = pkgs.fmt.override { stdenv = clangStdenv; };
      doxygenClang = pkgs.doxygen.overrideAttrs (old: {
        buildInputs = old.buildInputs ++ [
          llvm.libllvm
          pkgs.libclang
        ];
        cmakeFlags = old.cmakeFlags ++ ["-Duse_libclang=ON"];
      });

      docsDeps = with pkgs; [
        graphviz
        doxygenClang
        perl
        texliveBasic
      ];

      buildDeps = with pkgs; [
        llvm.libcxxClang
        cmake
        pkgconf
        ninja
      ];

      execDeps = with pkgs; [
        fmtLibcxx
        glfw
        fast-float
        range-v3
        hwloc
      ];

      pver = "1.0.0";
    in {
      devShells.${system} = {
        default = pkgs.mkShell {
          packages = with pkgs; [
            # profiling and debug
            perf
            libllvm
            lldb
          ] ++ execDeps ++ docsDeps ++ buildDeps;

          shellHook = ''
          export CMAKE_EXPORT_COMPILE_COMMANDS=1
          export NIX_ENFORCE_NO_NATIVE=0
          export ASAN_SYMBOLIZER_PATH=${pkgs.lib.getExe' pkgs.libllvm "llvm-symbolizer"}
          ulimit -s unlimited
          '';
        };

        clean = pkgs.mkShell {
          packages = with pkgs; [
            libGL
            libx11
            libxrandr
            libxinerama
            libxi
            libxext
            libxcursor
            libxxf86vm
            llvm.libcxx
          ] ++ buildDeps;

          shellHook = ''
          export CMAKE_EXPORT_COMPILE_COMMANDS=1
          export NIX_ENFORCE_NO_NATIVE=0
          export LIBCXX_DIR=${pkgs.lib.getStatic pkgs.llvmPackages.libcxx}
          ulimit -s unlimited
          '';
        };
      };

      packages.${system} = {
        docs = pkgs.stdenv.mkDerivation {
          pname = "loom-docs";
          version = pver;
          src = self;

          nativeBuildInputs = docsDeps ++ buildDeps ++ [ pkgs.clang ];

          cmakeFlags = [
            "-DBUILD_DOCS=ON"
            "-DBUILD_TESTS=OFF"
            "-DBUILD_BENCHMARKS=OFF"
            "-DBUILD_DEBUGGERS=OFF"
            "-DBUILD_DEMO=OFF"
            "-DBUILD_PROF=OFF"
          ];

          buildPhase = ''
          runHook preBuild
          cmake --build .
          runHook postBuild
          '';

          installPhase = ''
          mkdir -p $out
          cp -r html $out/html
          '';
        };

        default = pkgs.stdenv.mkDerivation {
          pname = "loom";
          version = pver;
          src = self;

          nativeBuildInputs = buildDeps;
          buildInputs = execDeps;

          cmakeFlags = [
            "-DBUILD_TESTS=OFF"
            "-DBUILD_BENCHMARKS=OFF"
            "-DBUILD_DEMO=ON"
          ];

          preBuild = ''
          export NIX_ENFORCE_NO_NATIVE=0
          ulimit -s unlimited
          '';
        };

        benchmarks = pkgs.stdenv.mkDerivation {
          pname = "loom";
          version = pver;
          src = self;

          nativeBuildInputs = buildDeps;
          buildInputs = execDeps;

          cmakeFlags = [
            "-DBUILD_TESTS=ON"
            "-DBUILD_BENCHMARKS=ON"
            "-DBUILD_DEMO=OFF"
          ];

          preBuild = ''
          export NIX_ENFORCE_NO_NATIVE=0
          ulimit -s unlimited
          '';

          buildPhase = ''
          runHook preBuild
          cmake --build . -t tests -j "$NIX_BUILD_CORES"
          cmake --build . -t light_bms -j "$NIX_BUILD_CORES"
          cmake --build . -t heavy_bms -j 1
          runHook postBuild
          '';

          doCheck = true;
          checkPhase = ''
          runHook preCheck
          ctest --output-on-failure
          runHook postCheck
          '';
        };
      };
    };
}
