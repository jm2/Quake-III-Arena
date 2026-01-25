#!/bin/bash
set -e

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
    chmod -R u+w "$RELEASE_ROOT" # Ensure we can overwrite
    
    # 1. Copy Binary
    if [ -f "build_mac/Quake3" ]; then
        cp "build_mac/Quake3" "$CONTENT_DIR/Quake 3 Arena/"
    else
        echo "Error: Binary build_mac/Quake3 not found."
        exit 1
    fi
    
    # 2. Asset Retrieval
    echo "Checking for assets..."
    # Search for pak0.pk3 anywhere in the current directory (max depth 3 to avoid slow scan)
    # Exclude release_mac to avoid finding our own copies from previous runs
    LOCAL_PAK=$(find . -maxdepth 3 -path "./$RELEASE_ROOT" -prune -o -name "pak0.pk3" -print | head -n 1)
    
    if [ -n "$LOCAL_PAK" ]; then
        echo "Found local pak0.pk3 at $LOCAL_PAK"
        cp "$LOCAL_PAK" "$CONTENT_DIR/Quake 3 Arena/baseq3/"
    else
        echo "Local pak0.pk3 not found in hierarchy."
        DEMO_INSTALLER="$TEMP_DIR/linuxq3ademo.sh"
        if [ ! -f "$DEMO_INSTALLER" ]; then
            echo "Downloading Quake 3 Demo assets..."
            # GWDG Mirror - Verified Working
            wget "https://ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake3/linux/linuxq3ademo-1.11-6.x86.gz.sh" -O "$DEMO_INSTALLER"
        fi
        
        echo "Extracting Demo Assets..."
        # Extract pak0.pk3 from the shell script installer
        # The data starts after line 165
        EXTRACT_DIR="$TEMP_DIR/extracted"
        mkdir -p "$EXTRACT_DIR"
        tail -n +165 "$DEMO_INSTALLER" | tar xz -C "$EXTRACT_DIR"
        
        # Locate pak0.pk3
        DEMO_PAK=$(find "$EXTRACT_DIR" -name "pak0.pk3" | head -n 1)
        if [ -n "$DEMO_PAK" ]; then
             cp "$DEMO_PAK" "$CONTENT_DIR/Quake 3 Arena/baseq3/"
             echo "Demo assets packaged."
        else
             echo "Error: Could not find pak0.pk3 in demo installer."
             chmod -R u+w "$TEMP_DIR" # Fix permissions before delete
             rm -rf "$TEMP_DIR"
             exit 1
        fi
        # We keep the temp dir for a moment until done, or clean up now.
        # Let's clean up extraction but maybe keep installer? No, clean all temp at end.
    fi

    # 3. Create HFS Image (Native HFS Volume)
    echo "Creating Native HFS Disk Image..."
    IMAGE_NAME="$RELEASE_ROOT/Quake3_Install.img"
    
    # Calculate required size (in KB) + 20MB buffer for HFS overhead
    CONTENT_SIZE=$(du -sk "$CONTENT_DIR" | awk '{print $1}')
    IMAGE_SIZE_KB=$((CONTENT_SIZE + 20480))
    
    echo "Allocating ${IMAGE_SIZE_KB}KB for HFS volume..."
    dd if=/dev/zero of="$IMAGE_NAME" bs=1024 count=$IMAGE_SIZE_KB status=none
    
    if ! command -v hformat &> /dev/null; then
        echo "Error: hfsutils not found. Cannot create native HFS image."
        echo "Please install 'hfsutils' (e.g., sudo apt-get install hfsutils)."
        exit 1
    fi
    
    # Format as HFS
    hformat -l "Quake 3 Arena" "$IMAGE_NAME" > /dev/null
    
    # Mount internal volume for hfsutils operations
    hmount "$IMAGE_NAME" > /dev/null
    
    # Recursive copy function for hfsutils using absolute HFS paths
    hcopy_recursive() {
        local local_dir="$1"
        local hfs_base="$2"
        for item in "$local_dir"/*; do
            if [ -d "$item" ]; then
                local folder_name=$(basename "$item")
                hmkdir "${hfs_base}${folder_name}" > /dev/null
                hcopy_recursive "$item" "${hfs_base}${folder_name}:"
            else
                local file_name=$(basename "$item")
                hcopy "$item" "${hfs_base}${file_name}" > /dev/null
            fi
        done
    }
    
    echo "Copying files to HFS volume..."
    cd "$CONTENT_DIR"
    hcopy_recursive "." ":"
    cd - > /dev/null
    
    # Set Metadata on the application binary
    echo "Setting Type/Creator metadata..."
    hattrib -t APPL -c 'Q3A ' ":Quake 3 Arena:Quake3" > /dev/null
    
    # Unmount
    humount > /dev/null
    
    echo "HFS Image created: $IMAGE_NAME"
    
    # MacBinary Encode the Image for classic Mac handling
    # Type: ???? (Generic/Unknown), Creator: dCpy (Disk Copy)
    # Using '????' forces Disk Copy to probe the filesystem rather than expect an NDIF header (dImg).
    echo "Encoding as MacBinary II..."
    BIN_NAME="$RELEASE_ROOT/Quake3_Install.img.bin"
    python3 macbinary_encode.py "$IMAGE_NAME" "$BIN_NAME" "????" "dCpy"
    
    if [ -f "$BIN_NAME" ]; then
        echo "Package created: $BIN_NAME"
        rm -f "$IMAGE_NAME" # Remove the raw image, we keep the bin
    else
        echo "Error: MacBinary encoding failed."
        exit 1
    fi
    
    # Cleanup
    echo "Cleaning up temp files..."
    chmod -R u+w "$TEMP_DIR"
    rm -rf "$TEMP_DIR"
fi
