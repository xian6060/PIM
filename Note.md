# 1. 回到專案目錄並刪除舊快取
cd ~/pim_workspace/PIM-malloc
rm -rf build

# 2. 確保環境變數都在
export UPMEM_HOME=~/pim_workspace/upmem-sdk
export DPU_TARGET=simulator
export PKG_CONFIG_PATH=$(dirname $(find ~/pim_workspace/upmem-sdk -name "dpu.pc" | head -n 1)):$PKG_CONFIG_PATH
export C_INCLUDE_PATH=$UPMEM_HOME/include/dpu:$UPMEM_HOME/include/api:$C_INCLUDE_PATH
export LIBRARY_PATH=$UPMEM_HOME/lib:$LIBRARY_PATH
export LD_LIBRARY_PATH=$UPMEM_HOME/lib:$LD_LIBRARY_PATH

# 3. 進入腳本目錄並執行編譯
cd hw/script
python3 build.py --upmem-home $UPMEM_HOME

# 4. 執行微基準測試 (Microbenchmark)
cd ~/pim_workspace/PIM-malloc/hw/build
./Tests/MicrobenchmarkTest/Host/Microbenchmark_test_host