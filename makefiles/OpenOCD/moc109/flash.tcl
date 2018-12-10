######################################
# Author : Snow Yang
# Date   : 2018-12-03
# Mail   : yangsw@mxchip.com
######################################

set MODE_REG 0x400000

set MODE_APP  0
set MODE_QC   1
set MODE_ATE  2
set MODE_JTAG 3

# TODO: read these data from ROM or RAM
set JCB_START 0x0400020
set JCB_FUNC $($JCB_START + 0x00)
set JCB_RET  $($JCB_START + 0x04)
set JCB_BUF  $($JCB_START + 0x08)
set JCB_BUF_SIZE 0x1008

set READ  0
set WRITE 1
set ERASE 2
set CRC   3

set SUCCESS 0
set ERROR   1
set GOING   2   

proc memread32 {address} {
    mem2array memar 32 $address 1
    return $memar(0)
}

proc load_image_bin {fname foffset address length } {
    # Load data from fname filename at foffset offset to
    # target at address. Load at most length bytes.
    load_image $fname [expr $address - $foffset] bin $address $length
}

proc jtagmode {} {
    soft_reset_halt
    mww $::MODE_REG $::MODE_JTAG
}

proc waitbusy { timeout } {
    mww $::JCB_RET $::GOING
    set ret $::GOING
    
    loop t 0 $timeout 1 {
        resume
        # TODO: increase buffer size to decrease CPU halt
        after 5
        halt
        set ret [memread32 $::JCB_RET]  
        if { $ret == $::SUCCESS } {
            return
        }
    }

    error "error"
    exit -1;
}

proc erase { addr size } {
    mww $::JCB_FUNC $::ERASE
    mww $($::JCB_BUF + 0x00) $addr
    mww $($::JCB_BUF + 0x04) $size

    # TODO: timeout = size * ms/k
    waitbusy 5000
}

proc write { file addr } {
    set size [file size $file]
    set bufsize $($::JCB_BUF_SIZE - 8)

    set remain $size
    while {$remain > 0} {
        set n $($remain > $bufsize ? $bufsize : $remain)
        mww $::JCB_FUNC $::WRITE
        mww $($::JCB_BUF + 0x00) $addr
        mww $($::JCB_BUF + 0x04) $n
        load_image_bin $file $($size - $remain) $($::JCB_BUF + 8) $n
        set remain $($remain - $n)
        set addr $($addr + $n)

        # TODO: timeout = n * ms/k
        waitbusy 1000
        puts $($size - $remain)
    }
}

proc read { file addr size } {
    exec echo -n > $file

    set bufsize $($::JCB_BUF_SIZE - 8)

    set remain $size
    set tmpfile "tmp.bin"
    while {$remain > 0} {
        set n $($remain > $bufsize ? $bufsize : $remain)
        mww $::JCB_FUNC $::READ
        mww $($::JCB_BUF + 0x00) $addr
        mww $($::JCB_BUF + 0x04) $n
        load_image_bin $file $($size - $remain) $($::JCB_BUF + 8) $n
        set remain $($remain - $n)
        set addr $($addr + $n)

        # TODO: timeout = n * ms/k
        waitbusy 1000

        dump_image $tmpfile $::JCB_BUF $n
        exec cat $tmpfile >> $file
        exec rm $tmpfile
    }
}

proc crc { addr size } {
    mww $::JCB_FUNC $::CRC
    mww $($::JCB_BUF + 0x00) $addr
    mww $($::JCB_BUF + 0x04) $size

    # TODO: timeout = size * ms/k
    waitbusy 5000

    puts [format %x [memread32 $::JCB_BUF]]
}

proc writefile { file addr } {
    set size [file size $file]
    erase $addr $size
    write $file $addr
}

proc readfile { file addr size } {
    read $file $addr $size
}