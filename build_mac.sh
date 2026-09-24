#!/bin/bash
set -e

usage() {
    cat <<EOF
Usage: $(basename "$0") [package|--package] [--base-only|--team-arena] [-h|--help]

  (no args)            Configure, build, and validate the launchable Quake3
                       application: build_mac/Quake3.bin (MacBinary),
                       Quake3.dsk (HFS image) and Quake3.ad + %Quake3.ad
                       (AppleDouble). No game data is needed.
  package, --package   Build, then add retail game data and assemble a
                       Mac OS 9 install image (.img.bin) under release_mac/.
  --base-only          Build only Quake3 (the default).
  --team-arena         Build Quake3 and Quake3_TeamArena.
  -h, --help           Show this message.
EOF
}

PACKAGE_MODE=0
TEAM_ARENA_MODE=OFF
PACKAGE_MODE_SET=0
TEAM_ARENA_MODE_SET=0
for argument in "$@"; do
    case "$argument" in
        package|--package)
            if [ "$PACKAGE_MODE_SET" -eq 1 ]; then
                echo "Error: package mode was specified more than once." >&2
                exit 2
            fi
            PACKAGE_MODE=1
            PACKAGE_MODE_SET=1
            ;;
        --base-only)
            if [ "$TEAM_ARENA_MODE_SET" -eq 1 ]; then
                echo "Error: choose exactly one of --base-only or --team-arena." >&2
                exit 2
            fi
            TEAM_ARENA_MODE=OFF
            TEAM_ARENA_MODE_SET=1
            ;;
        --team-arena)
            if [ "$TEAM_ARENA_MODE_SET" -eq 1 ]; then
                echo "Error: choose exactly one of --base-only or --team-arena." >&2
                exit 2
            fi
            TEAM_ARENA_MODE=ON
            TEAM_ARENA_MODE_SET=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Error: unknown argument '$argument'." >&2
            usage >&2
            exit 2
            ;;
    esac
done

function download_file() {
    local url="$1"
    local output="$2"

    if command -v curl &> /dev/null; then
        echo "Downloading $url using curl..."
        curl -L --progress-bar -o "$output" "$url"
    elif command -v wget &> /dev/null; then
        echo "Downloading $url using wget..."
        wget -q --show-progress -O "$output" "$url"
    else
        echo "Error: Neither curl nor wget found. Cannot download files."
        return 1
    fi
}

# Create build directory
mkdir -p build_mac
cd build_mac

# Check that the local toolchain is complete. A compiler by itself is not
# enough: renderer compilation needs the OpenGL SDK headers in Retro68's
# prepared (correctly packed) include tree, and linking needs its import
# library.
COMPILER="powerpc-apple-macos-gcc"
LOCAL_BIN="$(pwd)/../tools/Retro68-build/bin"
PPC_INCLUDE_DIR="$(pwd)/../tools/Retro68-build/powerpc-apple-macos/include"
RAW_OPENGL_INCLUDE_DIR="$(pwd)/../tools/Retro68-src/InterfacesAndLibraries/Interfaces/CIncludes"
OPENGL_SHARED_DIR="$(pwd)/../tools/Retro68-src/InterfacesAndLibraries/SharedLibraries"
OPENGL_STUB_LIB="$OPENGL_SHARED_DIR/libOpenGLLibraryStub.a"

install_prepared_opengl_support() {
    local header

    if [ ! -x "$LOCAL_BIN/$COMPILER" ]; then
        return 1
    fi

    if [ ! -f "$PPC_INCLUDE_DIR/gl.h" ] || [ ! -f "$PPC_INCLUDE_DIR/agl.h" ]; then
        if [ ! -f "$RAW_OPENGL_INCLUDE_DIR/gl.h" ] ||
           [ ! -f "$RAW_OPENGL_INCLUDE_DIR/agl.h" ]; then
            return 1
        fi

        echo "Installing missing OpenGL SDK headers into the prepared toolchain..."
        mkdir -p "$PPC_INCLUDE_DIR"
        for header in gl.h glu.h glm.h agl.h aglContext.h aglMacro.h \
                      aglRenderers.h glext.h GL_gl.h GL_glext.h GL_glut.h \
                      gliContext.h gliDispatch.h glut.h; do
            if [ -f "$RAW_OPENGL_INCLUDE_DIR/$header" ]; then
                cp "$RAW_OPENGL_INCLUDE_DIR/$header" "$PPC_INCLUDE_DIR/$header"
            fi
        done
    fi

    if [ ! -f "$OPENGL_STUB_LIB" ]; then
        if [ ! -x "$LOCAL_BIN/MakeImport" ] ||
           [ ! -f "$OPENGL_SHARED_DIR/OpenGLLibraryStub" ] ||
           [ ! -f "$OPENGL_SHARED_DIR/OpenGLLibraryStub.rsrc" ]; then
            return 1
        fi

        echo "Generating missing OpenGL import library..."
        cp "$OPENGL_SHARED_DIR/OpenGLLibraryStub.rsrc" \
           "$OPENGL_SHARED_DIR/%OpenGLLibraryStub"
        "$LOCAL_BIN/MakeImport" \
            "$OPENGL_SHARED_DIR/OpenGLLibraryStub" "$OPENGL_STUB_LIB"
    fi

    [ -f "$PPC_INCLUDE_DIR/gl.h" ] &&
        [ -f "$PPC_INCLUDE_DIR/agl.h" ] &&
        [ -f "$OPENGL_STUB_LIB" ]
}

if ! install_prepared_opengl_support; then
    echo "Retro68 compiler or prepared OpenGL SDK support not found."
    echo "Running setup_retro68.sh..."
    cd ..
    ./setup_retro68.sh
    cd build_mac

    if ! install_prepared_opengl_support; then
        echo "Error: Retro68 setup completed without the required compiler,"
        echo "prepared gl.h/agl.h headers, and OpenGL import library."
        exit 1
    fi
fi

# All of those files can exist while the tools cannot run, for example after a
# host OS upgrade removed a shared library they need (issue #269). Run them
# now: CMake would only report that the compiler identification is unknown.
# Status 1 means the toolchain is broken; any other status means the check
# itself could not run (for example no usable TMPDIR).
CHECK_STATUS=0
bash ../check_retro68.sh "$(cd .. && pwd)/tools/Retro68-build" || CHECK_STATUS=$?
if [ "$CHECK_STATUS" -eq 1 ]; then
    echo "Stopping before CMake: the Retro68 toolchain cannot build Quake3."
    exit 1
elif [ "$CHECK_STATUS" -ne 0 ]; then
    echo "Stopping before CMake: check_retro68.sh could not check the Retro68"
    echo "toolchain (exit status $CHECK_STATUS)."
    exit 1
fi

# Add local bin to PATH for this session
export PATH="$LOCAL_BIN:$PATH"

# Configure with CMake using Retro68 toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Retro68.toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TEAM_ARENA="$TEAM_ARENA_MODE"

# CMake caches this option across runs. Gate every post-build/package action on
# the configuration that produced this build, never on a possibly stale file.
TEAM_ARENA_CACHE_VALUE=$(
    sed -n 's/^BUILD_TEAM_ARENA:BOOL=//p' CMakeCache.txt | tail -n 1
)
case "$TEAM_ARENA_CACHE_VALUE" in
    1|ON|TRUE|YES|Y)
        BUILD_TEAM_ARENA_ENABLED=1
        ;;
    *)
        BUILD_TEAM_ARENA_ENABLED=0
        ;;
esac

# Build. CMake links each XCOFF image, converts it with MakePEF, and has Rez
# combine the PEF with code/mac's resources into a launchable application in
# host-independent containers (see CMakeLists.txt).
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Validate that a converted PEF is plausibly well-formed. Catches the case
# where MakePEF succeeds on a corrupt XCOFF input (e.g. when --gc-sections
# dropped sections it needs) and produces a PEF that loads but executes
# garbage. We cannot fully validate without running on Mac OS, but we can:
#   - confirm the PEF magic ("Joy!peff") in the first 8 bytes
#   - confirm the architecture tag is "pwpc"
#   - confirm the file is at least MIN_PEF_BYTES (an XCOFF for a Q3-sized
#     codebase with section-GC stripped produces a PEF well under this).
# Fail the build on any of these — silent corrupt PEFs are exactly the
# debugging tax we are trying to avoid.
validate_pef() {
    local pef_file="$1"
    local MIN_PEF_BYTES=1048576  # 1 MiB; Q3 PEFs are ~4-6 MiB in practice

    if [ ! -f "$pef_file" ]; then
        echo "PEF validation: $pef_file not found"
        return 1
    fi

    local magic1 magic2 arch size
    magic1=$(xxd -s 0 -l 4 -p "$pef_file" 2>/dev/null)
    magic2=$(xxd -s 4 -l 4 -p "$pef_file" 2>/dev/null)
    arch=$(xxd -s 8 -l 4 -p "$pef_file" 2>/dev/null)
    size=$(wc -c < "$pef_file")

    if [[ "$magic1" != "4a6f7921" ]]; then
        echo "PEF validation FAILED: $pef_file magic1='$magic1' (want 'Joy!' = 4a6f7921)"
        return 1
    fi
    if [[ "$magic2" != "70656666" ]]; then
        echo "PEF validation FAILED: $pef_file magic2='$magic2' (want 'peff' = 70656666)"
        return 1
    fi
    if [[ "$arch" != "70777063" ]]; then
        echo "PEF validation FAILED: $pef_file arch='$arch' (want 'pwpc' = 70777063)"
        return 1
    fi
    if [ "$size" -lt "$MIN_PEF_BYTES" ]; then
        echo "PEF validation FAILED: $pef_file is $size bytes (want >= $MIN_PEF_BYTES)"
        echo "  This usually means the XCOFF was missing sections MakePEF needs."
        return 1
    fi

    echo "PEF validation OK: $pef_file ($size bytes)"
    return 0
}

APPLICATIONS="Quake3"
if [ "$BUILD_TEAM_ARENA_ENABLED" -eq 1 ]; then
    APPLICATIONS="$APPLICATIONS Quake3_TeamArena"
fi

# Every build, not only package mode, must yield a launchable application:
# APPL/IDQ3 with kHasBundle, the PEF as its data fork, and cfrg/SIZE/BNDL/FREF
# and icon resources in every container.
for app in $APPLICATIONS; do
    validate_pef "$app.pef" || exit 1
    python3 ../mac_app.py verify --pef "$app.pef" \
        "$app.bin" "$app.dsk" "%$app.ad" || exit 1
done

echo "Build complete. Launchable Classic application(s) in build_mac/:"
for app in $APPLICATIONS; do
    echo "  $app.bin (MacBinary), $app.dsk (HFS disk image),"
    echo "  $app.ad + %$app.ad (AppleDouble)"
done

# Packaging Subcommand
if [ "$PACKAGE_MODE" -eq 1 ]; then
    cd .. # Return to project root
    echo "=========================================="
    echo "Packaging for Mac OS 9..."
    echo "=========================================="
    
    # Directory Structure
    # release_mac/
    #   content/  (The folders that go into the image)
    #   temp/     (Downloads, extractions, mapping file)
    #   (Output image goes to release_mac/Quake3_Install.img)
    
    RELEASE_ROOT="release_mac"
    CONTENT_DIR="$RELEASE_ROOT/content"
    TEMP_DIR="$RELEASE_ROOT/temp"
    BIN_CONTENT_DIR="$RELEASE_ROOT/bin_content"
    
    # These are generated staging trees. Reusing them can silently package
    # binaries or assets left by a different CMake configuration.
    rm -rf "$CONTENT_DIR" "$TEMP_DIR" "$BIN_CONTENT_DIR"
    mkdir -p "$CONTENT_DIR/Quake 3 Arena/baseq3"
    mkdir -p "$TEMP_DIR"
    chmod -R u+w "$RELEASE_ROOT" 2>/dev/null || true # Ensure we can overwrite
    
    # 1. Asset Retrieval Strategy. Only the game data needs retail files; the
    # applications themselves come from the build step above.
    echo "Checking for Game Assets..."
    
    # Check for baseq3/pak0.pk3 (look specifically for 'pak0.pk3' inside a 'baseq3' folder)
    # Check for baseq3/pak0.pk3 (look specifically for 'pak0.pk3' inside a 'baseq3' folder)
    PAK0_PATH=$(find . -path "*/baseq3/pak0.pk3" -not -path "*/release_mac/*" -print -quit)
    # Check for missionpack/pak0.pk3 (look specifically for 'pak0.pk3' inside a 'missionpack' folder)
    MP_PAK0_PATH=$(find . -path "*/missionpack/pak0.pk3" -not -path "*/release_mac/*" -print -quit)
    
    if [ -n "$PAK0_PATH" ]; then
        echo "Found Base Game Data: $PAK0_PATH"
        PAK0_DIR=$(dirname "$PAK0_PATH")
        find "$PAK0_DIR" -maxdepth 1 -type f -name "pak*.pk3" \
            -exec cp {} "$RELEASE_ROOT/content/Quake 3 Arena/baseq3/" \;
        
        # FIX: Copy menus.txt for older pak0 versions
        if [ -f "ui/menus.txt" ]; then
             mkdir -p "$RELEASE_ROOT/content/Quake 3 Arena/baseq3/ui"
             cp "ui/menus.txt" "$RELEASE_ROOT/content/Quake 3 Arena/baseq3/ui/"
             echo "Copied ui/menus.txt to baseq3/ui/"
        fi
        

        
        if [ "$BUILD_TEAM_ARENA_ENABLED" -eq 1 ]; then
             if [ -z "$MP_PAK0_PATH" ]; then
                 echo "Error: Team Arena is enabled but missionpack/pak0.pk3 was not found."
                 exit 1
             fi
             echo "Found Team Arena Data: $MP_PAK0_PATH"
             MP_PAK0_DIR=$(dirname "$MP_PAK0_PATH")
             mkdir -p "$RELEASE_ROOT/content/Quake 3 Arena/missionpack"
             find "$MP_PAK0_DIR" -maxdepth 1 -type f -name "pak*.pk3" \
                 -exec cp {} "$RELEASE_ROOT/content/Quake 3 Arena/missionpack/" \;
        fi
        
        # Check for Updates
        # Download if pak1 is missing in baseq3 OR if we have missionpack but missing its updates
        NEED_UPDATE=0
        if [ ! -f "$RELEASE_ROOT/content/Quake 3 Arena/baseq3/pak1.pk3" ]; then
            NEED_UPDATE=1
        fi
        if [ "$BUILD_TEAM_ARENA_ENABLED" -eq 1 ] &&
           [ ! -f "$RELEASE_ROOT/content/Quake 3 Arena/missionpack/pak1.pk3" ]; then
             NEED_UPDATE=1
        fi

        if [ "$NEED_UPDATE" -eq 1 ]; then
            echo "Downloading Updates (BaseQ3 / MissionPack)..."
            
            # User provided URL for latest pk3s
            PR_URL="https://files.ioquake3.org/quake3-latest-pk3s.zip"
            PR_FILE="quake3-latest-pk3s.zip"
            
            download_file "$PR_URL" "$TEMP_DIR/$PR_FILE"
            
            if [ -f "$TEMP_DIR/$PR_FILE" ]; then
                echo "Extracting Updates..."
                # Extract ZIP
                unzip -q -o "$TEMP_DIR/$PR_FILE" -d "$TEMP_DIR/pr_extract"
                
                echo "Copying BaseQ3 Updates..."
                find "$TEMP_DIR/pr_extract" -path "*/baseq3/pak*.pk3" -exec cp {} "$RELEASE_ROOT/content/Quake 3 Arena/baseq3/" \;
                
                if [ "$BUILD_TEAM_ARENA_ENABLED" -eq 1 ] && [ -n "$MP_PAK0_PATH" ]; then
                    echo "Copying MissionPack Updates..."
                    find "$TEMP_DIR/pr_extract" -path "*/missionpack/pak*.pk3" -exec cp {} "$RELEASE_ROOT/content/Quake 3 Arena/missionpack/" \;
                fi
            else
                echo "Warning: Failed to download Updates."
            fi
        fi
    else
        echo "Error: a retail baseq3/pak0.pk3 was not found." >&2
        echo "The demo pak belongs to demoq3 and is not compatible with this full-game build." >&2
        echo "Place legally obtained retail data under a baseq3 directory and retry." >&2
        exit 1
    fi

    # 2. Create HFS/ISO Hybrid Image
    # We revert to genisoimage as hfsutils wrappers caused mounting errors (-8819/-8816).
    # genisoimage produces a valid hybrid (ISOf) that Disk Copy 6.5 can read.
    echo "Creating HFS/ISO Hybrid Image..."
    IMAGE_NAME="$RELEASE_ROOT/Quake3_Install.img"
    MAPPING_FILE="$TEMP_DIR/hfs_mapping.txt"
    
    # Create a mapping file for HFS creator/types
    cat > "$MAPPING_FILE" <<EOF
.pk3   Raw   IDQ3  Stak "Quake 3 Data"
.cfg   Ascii IDQ3  TEXT "Quake 3 Config"
Quake3 Raw   IDQ3  APPL "Quake 3 App"
Quake3_TeamArena Raw IDQ3 APPL "Quake 3 Team Arena"
EOF
    
    if [ "$(uname)" == "Darwin" ] && command -v hdiutil &> /dev/null; then
         MKISOFS="hdiutil"
    elif command -v genisoimage &> /dev/null; then
        MKISOFS="genisoimage"
    elif command -v mkisofs &> /dev/null; then
        MKISOFS="mkisofs"
    else
         echo "Error: genisoimage/mkisofs/hdiutil not found."
         exit 1
    fi
    
    # Stage an application the build step produced. Rez already wrote it in
    # host-independent containers, so packaging neither recompiles resources
    # nor depends on how Rez stores resource forks on this host.
    stage_application() {
        local name="$1"
        local destination="$2"
        local container finder_info

        for container in "build_mac/$name.bin" "build_mac/$name.ad" \
                         "build_mac/%$name.ad"; do
            if [ ! -s "$container" ]; then
                echo "Error: $container is missing; rebuild before packaging." >&2
                exit 1
            fi
        done

        cp "build_mac/$name.ad" "$destination/$name"
        if [ "$MKISOFS" == "hdiutil" ]; then
            # hdiutil reads real forks: export them from the MacBinary.
            python3 mac_app.py resource-fork "build_mac/$name.bin" \
                "$TEMP_DIR/$name.rsrc"
            cat "$TEMP_DIR/$name.rsrc" > "$destination/$name/..namedfork/rsrc"
            finder_info=$(python3 mac_app.py finder-info "build_mac/$name.bin")
            xattr -wx com.apple.FinderInfo "$finder_info" "$destination/$name"
        else
            # genisoimage/mkisofs -double merges %Name into Name's forks.
            cp "build_mac/%$name.ad" "$destination/%$name"
        fi
    }

    # Confirm an HFS image holds every application intact. hdiutil writes
    # HFS+, which mac_app.py does not read.
    verify_image_applications() {
        local image="$1"
        local app

        if [ "$MKISOFS" != "hdiutil" ]; then
            for app in $APPLICATIONS; do
                python3 mac_app.py verify --hfs-name "$app" \
                    --pef "build_mac/$app.pef" "$image"
            done
        fi
    }

    for app in $APPLICATIONS; do
        stage_application "$app" "$CONTENT_DIR/Quake 3 Arena"
    done

    # 3. Copy all pak*.pk3 files recursively
    # (Already handled in Step 1)

     # Create Hybrid Image
     rm -f "$IMAGE_NAME" "${IMAGE_NAME}.iso"
     
     if [ "$MKISOFS" == "hdiutil" ]; then
         # Native macOS approach
         echo "Using hdiutil to create hybrid image..."
         
         # hdiutil makehybrid
         # -hfs creates a hybrid (or pure HFS+ if others omitted).
         # We omit -joliet -iso to prevent the dual-disk issue (lowercase/uppercase volumes).
         # -hfs-volume-name sets the HFS volume name.
         hdiutil makehybrid -o "$IMAGE_NAME" -hfs -default-volume-name "Quake 3 Arena" "$CONTENT_DIR"
         
         if [ -f "${IMAGE_NAME}.iso" ]; then
             mv "${IMAGE_NAME}.iso" "$IMAGE_NAME"
         elif [ -f "${IMAGE_NAME}.dmg" ]; then
             mv "${IMAGE_NAME}.dmg" "$IMAGE_NAME"
         fi
         
    else
         # mkisofs strategy
         # Create Hybrid Image (No -part, as that might confuse simple mounting)
         # -hfs automatically detects AppleDouble (._ file) if present (in most versions).
         echo "Using mkisofs/genisoimage..."
         $MKISOFS -hfs -double -map "$MAPPING_FILE" -o "$IMAGE_NAME" -V "Quake 3 Arena" "$CONTENT_DIR"
    fi
    
    verify_image_applications "$IMAGE_NAME"
    echo "HFS Image created: $IMAGE_NAME"
    
    # MacBinary Encode
    # Type: 'iso ' (ISO Image), Creator: dCpy (Disk Copy)
    # This matches the hybrid format and should allow generic mounting.
    echo "Encoding as MacBinary II..."
    BIN_NAME="$RELEASE_ROOT/Quake3_Install.img.bin"
    python3 macbinary_encode.py "$IMAGE_NAME" "$BIN_NAME" "iso " "dCpy"
    
    # ---------------------------------------------------------
    # Create Binary-Only Image (for faster deployment/testing)
    # ---------------------------------------------------------
    echo "Creating Binaries-Only Image..."
    BIN_IMG_NAME="$RELEASE_ROOT/Quake3_Bin.img"
    mkdir -p "$BIN_CONTENT_DIR"
    
    for app in $APPLICATIONS; do
        stage_application "$app" "$BIN_CONTENT_DIR"
    done

    # FIX: Include ui/menus.txt in binaries image too
    if [ -f "ui/menus.txt" ]; then
         mkdir -p "$BIN_CONTENT_DIR/baseq3/ui"
         cp "ui/menus.txt" "$BIN_CONTENT_DIR/baseq3/ui/"
    fi


    
    if [[ "$MKISOFS" != "hdiutil" ]]; then
         $MKISOFS -hfs -double -map "$MAPPING_FILE" -o "$BIN_IMG_NAME" -V "Quake 3 Binaries" "$BIN_CONTENT_DIR"
    else
         # Same HFS-only hybrid as the install image; -joliet -iso would
         # reintroduce the dual-volume mount.
         hdiutil makehybrid -o "$BIN_IMG_NAME" -hfs -default-volume-name "Quake 3 Binaries" "$BIN_CONTENT_DIR"
         if [ -f "${BIN_IMG_NAME}.iso" ]; then
             mv "${BIN_IMG_NAME}.iso" "$BIN_IMG_NAME"
         elif [ -f "${BIN_IMG_NAME}.dmg" ]; then
             mv "${BIN_IMG_NAME}.dmg" "$BIN_IMG_NAME"
         fi
    fi
    
    if [ -f "$BIN_IMG_NAME" ]; then
         verify_image_applications "$BIN_IMG_NAME"
         echo "Encoding Binaries Image..."
         python3 macbinary_encode.py "$BIN_IMG_NAME" "${BIN_IMG_NAME}.bin" "iso " "dCpy"
         echo "Package created: ${BIN_IMG_NAME}.bin"
         rm -f "$BIN_IMG_NAME"
    fi
    
    if [ -f "$BIN_NAME" ]; then
        echo "Package created: $BIN_NAME"
        rm -f "$IMAGE_NAME"
    else
        echo "Error: MacBinary encoding failed."
        exit 1
    fi
    
    # Cleanup
    echo "Cleaning up temp files..."
    chmod -R u+w "$TEMP_DIR"
    rm -rf "$TEMP_DIR"
fi
