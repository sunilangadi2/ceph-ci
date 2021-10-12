#!/bin/sh -ex
#
# cephfs_mirror_ha.sh - test cephfs-mirror daemons in HA mode
#

REPO=ceph-qa-suite
REPO_DIR=ceph_repo
REPO_PATH_PFX="$REPO_DIR/$REPO"

NR_DIRECTORIES=4
NR_SNAPSHOTS=4

exec_git_cmd()
{
    local repo=$1
    local cmd=("$@")
    echo $(git --git-dir "http://github.com/ceph/$repo" ${cmd[0]})
}

clone_repo()
{
    local repo_name=$1
    echo $(git clone --branch giant "http://github.com/ceph/$REPO" $repo_name)
}

setup_repos()
{
    echo $(mkdir "$REPO_DIR")

    for i in `seq 1 $NR_DIRECTORIES`
    do
        local repo_name="${REPO_PATH_PFX}_$i"
        echo $(mkdir $repo_name)
        clone_repo($repo_name)
    done
}

configure_peer()
{
    echo $(ceph mgr module enable mirroring)
    echo $(ceph fs snapshot mirror enable cephfs)
    echo $(ceph fs snapshot mirror peer_add cephfs client.mirror_remote@ceph ceph.backup_fs)

    for i in `seq 1 $NR_DIRECTORIES`
    do
        local repo_name="${REPO_PATH_PFX}_$i"
        echo $(ceph fs snapshot mirror add cephfs "/$repo_name")
    done
}

create_snaps()
{
    for i in `seq 1 $NR_DIRECTORIES`
    do
        local repo_name="${REPO_PATH_PFX}_$i"
        for j in `seq 1 $NR_SNAPSHOTS`
        do
            exec_git_cmd(repo_name, ("pull"))
            r=$(( $RANDOM % 100 + 5 ))
            exec_git_cmd(repo_name, ("reset", "--hard", "HEAD~$r"))
            echo $(mkdir "$repo_name/.snap/snap_$j")
        done
}

# setup git repos to be used as data set
setup_repos()

# turn on mirroring, add peers...
configure_peer()

# snapshots on primary
create_snaps()
