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
      ];

      execDeps = with pkgs; [
        fmtLibcxx
        glfw
        fast-float
        range-v3
        hwloc
      ];

      cleanDeps = with pkgs; [
            libGL
            libx11
            libxrandr
            libxinerama
            libxi
            libxext
            libxcursor
            libxxf86vm
            llvm.libcxx
          ];

      pver = "1.0.0";

      prepare = ''
        export NIX_ENFORCE_NO_NATIVE=0
        ulimit -s unlimited
      '';

      safeBuild = ''
        runHook preBuild
        cmake --build . -t light_targets -j "$NIX_BUILD_CORES"
        cmake --build . -t heavy_targets -j 1
        runHook postBuild
      '';

      runTests = ''
        runHook preCheck
        ctest --output-on-failure
        runHook postCheck
      '';

      buildForArch = { name, arch, features ? [] }:
        let
          wideOn = if name == "sse" then "OFF" else "ON";
          glfwSrc = pkgs.fetchFromGitHub {
            owner = "glfw";
            repo = "glfw";
            tag = "3.3.8";
            hash = "sha256-4+H0IXjAwbL5mAWfsIVhW0BSJhcWjkQx4j2TrzZ3aIo=";
          };
          hwlocSrc = pkgs.fetchzip {
            url = "https://download.open-mpi.org/release/hwloc/v2.13/hwloc-2.13.0.tar.bz2";
            hash = "sha256-GdVv3Og4OvrZCgV8StfHldTX4NSwXC61S4tZsfLsEEU=";
          };
        in
          pkgs.stdenv.mkDerivation {
            pname = "loom-${name}";
            version = pver;
            src = self;

            nativeBuildInputs = buildDeps ++ [ pkgs.patchelf ];
            buildInputs = with pkgs; [ fmt range-v3 fast-float ] ++ cleanDeps;

            cmakeFlags = [
              "-DBUILD_DOCS=OFF"
              "-DBUILD_TESTS=ON"
              "-DBUILD_BENCHMARKS=ON"
              "-DBUILD_HEAVY_BENCHMARKS=${wideOn}"
              "-DBUILD_DEBUGGERS=OFF"
              "-DBUILD_DEMO=${wideOn}"
              "-DBUILD_PROF=OFF"
              "-DTARGET_CPU=${arch}"
              "-DTARGET_CPU_FEATURES=${pkgs.lib.strings.join ";" features}"
              "-DBINDIST=ON"
              "-DLIBCXX_DIR=${pkgs.lib.getStatic pkgs.llvmPackages.libcxx}"
              "-DGLFW_SRC_DIR=${glfwSrc}"
              "-DHWLOC_SRC_DIR=${hwlocSrc}"
            ];

            preBuild = ''
              ulimit -s unlimited
            '';
            buildPhase = safeBuild;
            doCheck = true;
            checkPhase = runTests;

            postFixup = ''
              for exe in $out/bin/*; do
                patchelf --remove-rpath "$exe"
                patchelf --set-interpreter /lib64/ld-linux-x86-64.so.2 "$exe"
              done
            '';

            postInstall = ''
              strip $out/bin/*
            '';
          };
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
          export ASAN_SYMBOLIZER_PATH=${pkgs.lib.getExe' pkgs.libllvm "llvm-symbolizer"}
          '' + prepare;
        };

        clean = pkgs.mkShell {
          packages = cleanDeps ++ buildDeps;

          shellHook = ''
          export LIBCXX_DIR=${pkgs.lib.getStatic pkgs.llvmPackages.libcxx}
          '' + prepare;
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
            "-DBUILD_TESTS=ON"
            "-DBUILD_BENCHMARKS=ON"
            "-DBUILD_HEAVY_BENCHMARKS=OFF"
            "-DBUILD_DEMO=ON"
          ];

          preBuild = prepare;
          buildPhase = safeBuild;
          doCheck = true;
          checkPhase = runTests;
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
            "-DBUILD_HEAVY_BENCHMARKS=ON"
            "-DBUILD_DEMO=OFF"
          ];

          preBuild = prepare;
          buildPhase = safeBuild;
          doCheck = true;
          checkPhase = runTests;
        };
      } // builtins.listToAttrs (map
        (cfg: {
          name = cfg.name;
          value = buildForArch cfg;
        })
        [
          { name = "sse"; arch = "x86-64-v2"; }
          { name = "avx"; arch = "x86-64-v2"; features = [ "avx" ]; }
          { name = "avx2"; arch = "x86-64-v3"; }
          { name = "avx512"; arch = "x86-64-v4"; }
          { name = "sandy"; arch = "sandybridge"; }
        ]);
    };
}
