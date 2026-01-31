#!/bin/bash
set -e

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

# Check if compiler exists in PATH or local dir
COMPILER="powerpc-apple-macos-gcc"
LOCAL_BIN="$(pwd)/../tools/Retro68-build/bin"

if ! command -v "$COMPILER" &> /dev/null && [ ! -x "$LOCAL_BIN/$COMPILER" ]; then
    echo "Retro68 compiler not found."
    echo "Running setup_retro68.sh..."
    cd ..
    ./setup_retro68.sh
    cd build_mac
fi

# Add local bin to PATH for this session
export PATH="$LOCAL_BIN:$PATH"

# Configure with CMake using Retro68 toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Retro68.toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Convert to PEF (Fix for XCOFF output)
MAKEPEF="../tools/Retro68-build/bin/MakePEF"
if [ -f "$MAKEPEF" ]; then
    echo "Checking binaries for PEF conversion..."
    
    if [ -f "Quake3" ]; then
        # Check if already PEF (Joy! header) using xxd
        MAGIC=$(xxd -l 4 -p Quake3 2>/dev/null || echo "00000000")
        if [[ "$MAGIC" != "4a6f7921" ]]; then
             echo "Converting Quake3 to PEF..."
             "$MAKEPEF" Quake3 -o Quake3.pef
             mv Quake3.pef Quake3
             echo "Created Quake3 (PEF)"
        else
             echo "Quake3 is already PEF."
        fi
    fi
    
    if [ -f "Quake3_TeamArena" ]; then
        MAGIC=$(xxd -l 4 -p Quake3_TeamArena 2>/dev/null || echo "00000000")
        if [[ "$MAGIC" != "4a6f7921" ]]; then
             echo "Converting Quake3_TeamArena to PEF..."
             "$MAKEPEF" Quake3_TeamArena -o Quake3_TeamArena.pef
             mv Quake3_TeamArena.pef Quake3_TeamArena
             echo "Created Quake3_TeamArena (PEF)"
        else
             echo "Quake3_TeamArena is already PEF."
        fi
    fi
else
    echo "Warning: MakePEF not found. Binaries might be invalid XCOFF."
fi

echo "Build complete. Check build_mac/Quake3 or similar."

# Packaging Subcommand
if [[ "$1" == "package" ]]; then
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
    
    mkdir -p "$CONTENT_DIR/Quake 3 Arena/baseq3"
    mkdir -p "$TEMP_DIR"
    chmod -R u+w "$RELEASE_ROOT" 2>/dev/null || true # Ensure we can overwrite
    
    # 1. Copy Binary
    if [ -f "build_mac/Quake3" ]; then
        cp "build_mac/Quake3" "$CONTENT_DIR/Quake 3 Arena/"
    else
        echo "Error: Binary build_mac/Quake3 not found."
        exit 1
    fi
    
    # 1. Asset Retrieval Strategy
    echo "Checking for Game Assets..."
    
    # Check for baseq3/pak0.pk3 (look specifically for 'pak0.pk3' inside a 'baseq3' folder)
    # Check for baseq3/pak0.pk3 (look specifically for 'pak0.pk3' inside a 'baseq3' folder)
    PAK0_PATH=$(find . -path "*/baseq3/pak0.pk3" -not -path "*/release_mac/*" -print -quit)
    # Check for missionpack/pak0.pk3 (look specifically for 'pak0.pk3' inside a 'missionpack' folder)
    MP_PAK0_PATH=$(find . -path "*/missionpack/pak0.pk3" -not -path "*/release_mac/*" -print -quit)
    
    if [ -n "$PAK0_PATH" ]; then
        echo "Found Base Game Data: $PAK0_PATH"
        cp "$PAK0_PATH" "$RELEASE_ROOT/content/Quake 3 Arena/baseq3/"
        
        # FIX: Copy menus.txt for older pak0 versions
        if [ -f "ui/menus.txt" ]; then
             mkdir -p "$RELEASE_ROOT/content/Quake 3 Arena/baseq3/ui"
             cp "ui/menus.txt" "$RELEASE_ROOT/content/Quake 3 Arena/baseq3/ui/"
             echo "Copied ui/menus.txt to baseq3/ui/"
        fi
        

        
        if [ -n "$MP_PAK0_PATH" ]; then
             echo "Found Team Arena Data: $MP_PAK0_PATH"
             mkdir -p "$RELEASE_ROOT/content/Quake 3 Arena/missionpack"
             cp "$MP_PAK0_PATH" "$RELEASE_ROOT/content/Quake 3 Arena/missionpack/"
        fi
        
        # Check for Updates
        # Download if pak1 is missing in baseq3 OR if we have missionpack but missing its updates
        NEED_UPDATE=0
        if [ ! -f "pak1.pk3" ] && [ ! -f "$RELEASE_ROOT/content/Quake 3 Arena/baseq3/pak1.pk3" ]; then
            NEED_UPDATE=1
        fi
        if [ -n "$MP_PAK0_PATH" ] && [ ! -f "missionpack/pak1.pk3" ] && [ ! -f "$RELEASE_ROOT/content/Quake 3 Arena/missionpack/pak1.pk3" ]; then
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
                
                if [ -n "$MP_PAK0_PATH" ]; then
                    echo "Copying MissionPack Updates..."
                    find "$TEMP_DIR/pr_extract" -path "*/missionpack/pak*.pk3" -exec cp {} "$RELEASE_ROOT/content/Quake 3 Arena/missionpack/" \;
                fi
            else
                echo "Warning: Failed to download Updates."
            fi
        fi
    else
        echo "Local pak0.pk3 (Full Game) not found. Falling back to Demo assets..."
        DEMO_URL="https://ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake3/linux/linuxq3ademo-1.11-6.x86.gz.sh"
        DEMO_FILE="$TEMP_DIR/linuxq3ademo.sh"
        
        echo "Downloading Quake 3 Arena Demo..."
        download_file "$DEMO_URL" "$DEMO_FILE"
        
        if [ -f "$DEMO_FILE" ]; then
            echo "Extracting Demo Assets..."
            # Manually extract to avoid issues with old makeself script on macOS
            mkdir -p "$TEMP_DIR/demo_extract"
            tail -n +165 "$DEMO_FILE" | gzip -cd | tar xf - -C "$TEMP_DIR/demo_extract"
            
            if [ -f "$TEMP_DIR/demo_extract/demoq3/pak0.pk3" ]; then
                echo "Found Demo pak0.pk3"
                cp "$TEMP_DIR/demo_extract/demoq3/pak0.pk3" "$RELEASE_ROOT/content/Quake 3 Arena/baseq3/"
            else
                echo "Error: Could not extract demo pak0.pk3."
                exit 1
            fi
        else
            echo "Error: Demo download failed."
            exit 1
        fi
    fi

    # Icon Generation
    # Check if we need to generate icons (if quake3_icons.r doesn't exist)
    if [ ! -f "code/mac/quake3_icons.r" ] || [ ! -s "code/mac/quake3_icons.r" ]; then
        if [ -f "code/mac/quake3.svg" ] && command -v convert &> /dev/null; then
            echo "Generating Icons from SVG..."
            convert -background none -resize 32x32 code/mac/quake3.svg code/mac/quake3_32.png
            convert -background none -resize 16x16 code/mac/quake3.svg code/mac/quake3_16.png
            
            if [ -f "code/mac/quake3_32.png" ] && [ -f "code/mac/quake3_16.png" ]; then
                 python3 generate_icon_r.py code/mac/quake3_32.png code/mac/quake3_16.png > code/mac/quake3_icons.r
            fi
        fi
    else
        echo "Icons already generated (code/mac/quake3_icons.r exists)."
    fi

    # Manually compile Mac Resources
    echo "Compiling Mac Resources..."
    # Ensure quake3_icons.r exists (create dummy if missing to avoid Rez error)
    if [ ! -f "code/mac/quake3_icons.r" ]; then
        echo "/* No icons */" > "code/mac/quake3_icons.r"
    fi
    
    tools/Retro68-build/bin/Rez -o build_mac/Quake3.rsrc -I tools/Retro68-src/InterfacesAndLibraries/Interfaces/RIncludes -I code/mac code/mac/mac_resources.r
    
    # 3. Create HFS/ISO Hybrid Image
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
    
    # Prepare Content:
    # 1. Copy Binary
    cp build_mac/Quake3 "$RELEASE_ROOT/content/Quake 3 Arena/Quake3"
    
    if [ -f "build_mac/Quake3_TeamArena" ]; then
         cp build_mac/Quake3_TeamArena "$RELEASE_ROOT/content/Quake 3 Arena/Quake3_TeamArena"
    fi
    
    # 2. Generate AppleDouble for Resource Fork
    # Only needed for mkisofs strategy or if we want to retain them generally.
    # We generate them here for compatibility, but hdiutil path handles them natively.
    
    # Retro68 Rez might output to .rsrc subdirectory
    RSRC_FILE="build_mac/Quake3.rsrc"
    if [ -f "build_mac/.rsrc/Quake3.rsrc" ]; then
        RSRC_FILE="build_mac/.rsrc/Quake3.rsrc"
    fi

    if [ -f "$RSRC_FILE" ] && [ -s "$RSRC_FILE" ]; then
        echo "Generating AppleDouble resource fork for mkisofs (from $RSRC_FILE)..."
        # mkisofs expects '%' prefix for AppleDouble files to merge them into the resource fork
        python3 create_appledouble.py "$RSRC_FILE" "$RELEASE_ROOT/content/Quake 3 Arena/%Quake3"
        
        if [ -f "$RELEASE_ROOT/content/Quake 3 Arena/Quake3_TeamArena" ]; then
             cp "$RELEASE_ROOT/content/Quake 3 Arena/%Quake3" "$RELEASE_ROOT/content/Quake 3 Arena/%Quake3_TeamArena"
        fi
    else
        echo "Warning: Quake3.rsrc not found or empty. Application icon/type/creator/cfrg might be missing."
    fi

    # 3. Copy all pak*.pk3 files recursively
    # (Already handled in Step 1)

     # Create Hybrid Image
     rm -f "$IMAGE_NAME" "${IMAGE_NAME}.iso"
     
     if [ "$MKISOFS" == "hdiutil" ]; then
         # Native macOS approach
         echo "Using hdiutil to create hybrid image..."
         
         # 1. Apply resource forks natively
         if [ -f "$RSRC_FILE" ]; then
             echo "Applying resource forks natively for hdiutil (from $RSRC_FILE)..."
             # Apply to base binary
             cat "$RSRC_FILE" > "$RELEASE_ROOT/content/Quake 3 Arena/Quake3/..namedfork/rsrc"
             
             # Apply to Team Arena binary
             if [ -f "$RELEASE_ROOT/content/Quake 3 Arena/Quake3_TeamArena" ]; then
                  cat "$RSRC_FILE" > "$RELEASE_ROOT/content/Quake 3 Arena/Quake3_TeamArena/..namedfork/rsrc"
             fi
             
             # Set Type/Creator Codes
             # Quake3: APPL Q3A (0x51334120)
             # We use python to set FinderInfo if SetFile is missing
             echo "Setting FinderInfo (Type/Creator)..."
             # Type: APPL (0x4150504C), Creator: IDQ3 (0x49445133) followed by 24 bytes of zeros
             # Hex: 4150504C49445133000000000000000000000000000000000000000000000000
             FINDER_INFO_HEX="4150504C49445133000000000000000000000000000000000000000000000000"
             
             xattr -wx com.apple.FinderInfo "$FINDER_INFO_HEX" "$RELEASE_ROOT/content/Quake 3 Arena/Quake3"
             if [ -f "$RELEASE_ROOT/content/Quake 3 Arena/Quake3_TeamArena" ]; then
                 xattr -wx com.apple.FinderInfo "$FINDER_INFO_HEX" "$RELEASE_ROOT/content/Quake 3 Arena/Quake3_TeamArena"
             fi
         fi
         
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
    BIN_CONTENT_DIR="$RELEASE_ROOT/bin_content"
    mkdir -p "$BIN_CONTENT_DIR"
    
    # Copy Binaries and AppleDouble resource forks (for linux mkisofs)
    # If using hdiutil, resources are inside the file, but we can't easily split them on Linux without hfsutils.
    # We assume mkisofs strategy here since we are on Linux.
    
    cp "$CONTENT_DIR/Quake 3 Arena/Quake3" "$BIN_CONTENT_DIR/"
    if [ -f "$CONTENT_DIR/Quake 3 Arena/%Quake3" ]; then
        cp "$CONTENT_DIR/Quake 3 Arena/%Quake3" "$BIN_CONTENT_DIR/"
    fi
    
    if [ -f "$CONTENT_DIR/Quake 3 Arena/Quake3_TeamArena" ]; then
        cp "$CONTENT_DIR/Quake 3 Arena/Quake3_TeamArena" "$BIN_CONTENT_DIR/"
        if [ -f "$CONTENT_DIR/Quake 3 Arena/%Quake3_TeamArena" ]; then
             cp "$CONTENT_DIR/Quake 3 Arena/%Quake3_TeamArena" "$BIN_CONTENT_DIR/"
        fi
    fi

    # FIX: Include ui/menus.txt in binaries image too
    if [ -f "ui/menus.txt" ]; then
         mkdir -p "$BIN_CONTENT_DIR/baseq3/ui"
         cp "ui/menus.txt" "$BIN_CONTENT_DIR/baseq3/ui/"
    fi


    
    if [[ "$MKISOFS" != "hdiutil" ]]; then
         $MKISOFS -hfs -double -map "$MAPPING_FILE" -o "$BIN_IMG_NAME" -V "Quake 3 Binaries" "$BIN_CONTENT_DIR"
    else
         # On macOS, native copy works
         hdiutil makehybrid -o "$BIN_IMG_NAME" -hfs -joliet -iso -default-volume-name "Quake 3 Binaries" "$BIN_CONTENT_DIR"
         if [ -f "${BIN_IMG_NAME}.iso" ]; then mv "${BIN_IMG_NAME}.iso" "$BIN_IMG_NAME"; fi
    fi
    
    if [ -f "$BIN_IMG_NAME" ]; then
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
