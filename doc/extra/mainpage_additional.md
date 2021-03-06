Command line utilities
======================

Descriptions and usage about the included command line utilities can be found at the [Command line utilities](utils.html) page

Running the examples
====================

There are four examples prepared for you in a easy-to-run fashion. Each of the
example represents a figure in the article. In order reproduce the figures,
make sure you have R installed, and the binary can be found through PATH
lookup. If you run the following command

	which R

And see an absolute path, such as '/usr/bin/R', then the requirement is
satisfied

Getting the example tarball
---------------------------

The example tarball consists of four results presented in the paper. The
tarball is at the root directory of the source tree.

https://github.com/yiq/SubcloneSeeker/blob/master/examples.tar.gz



Setup the variables
-------------------

Navigate to the example directory on a commandline. Edit the file 'setup.sh' so
that it reflects how you organized the project files. Make sure that you have
compiled the binaries in the SubcloneSeeker project. If you haven't done so,
please refer to the 'INSTALLATION' section in this document.

###Example-1: Simulation

__Directory: 01-simulation__

The first example simulates 1000 tumors, each with 3, 4, ..., 8 subclones.
Subclone Deconvolution is then performed on all the samples, and statistics
will be collected as in how many possible solutions for each tumor. You can
initiate the simulation by running the 'sim.sh' script

	./sim.sh

Note that this will take some time to finish, especially in the case 7 or 8
subclones per tumor. Results will be stored in 'result' directory. To plot the
results, launch R in the example directory and run the 'plot.r' scriot

	R
	source('plot.r');

A result set from a run executed on my computer is also provided, if you just
want to plot the results. In that case, please use the 'plot\_prerun.r' script

###Example-2: Purity estimation with the snuc cancer cell-line dataset

__Directory: 02-snuc__

In this example, a snuc cancer cell line (thus very high purity) was
sequenced, and its sequencing reads were combined with those reads from
sequencing the paired-normal sample at various ratios. Subclone deconvolution
is performed on each of the digitally created samples, and purity estimation is
reported. In order to run the example, simple type

	make

in the example directory, as it will take care of removing old results (if you
run it before), creating required directories, running the deconvolution and
plotting the result. After the execution, a file 'SamplePurity.pdf' should be
created in the same directory, which contains the plotted result. 

###Example-3: Re-analysis of the dataset presented in Ding et, al. 2012

execute

	make

As an example, the following is part of the output

    SAMPLE UPN933124:
    Primary tree 11 is compatible with Secondary tree 1

This means that, for the sample UPN933124, the primary tree, with a root node whose id is 11, is compatible with the relapse (secondary) tree, with a root node whose id is 1. To further investigate the actual structures, the utility `treeprint` can be used. The following commands assume that an environment variable `SSHOME` exists that points to the directory of SubcloneSeeker

    $SSHOME/utils/treeprint -l results/subclones/UPN933124-pri.sqlite
    1
    6
    11
    16
    21
    26

There are 6 subclone structures (or root nodes, more precisely) in the database resulted from reconstruction on the primary sample. Since tree 11 is compatible, let's look at that one particularly

    $SSHOME/utils/treeprint -r 11 results/subclones/UPN933124-pri.sqlite
    0,(0.127401,(0.531157,0.29044,(0.051003,)))

It indicates that the structure of tree 11 is as follows:

  * The root node, which represent normal tissue, is 0% (because the data was adjusted by the highest AF before subjected to reconstruction)
  * The next subclone, whose parent is the normal clone, has a subclone frequency of 12.74% (let's denote it as subclone A)
  * The next two subclones are both the children of subclone A, each taking up 53.11% and 29.04% of the entire population (denoted as subclone B and C)
  * The next subclone, whose parent is subclone C, takes up 5.1% of the entire population

A more visualization friendly output can be produced with the `-g` option with `treeprint`, which print the structure in graphviz format

    $SSHOME/utils/treeprint -r 11 -g results/subclones/UPN933124-pri.sqlite
    digraph {
        n11 [label="n11: 0%"];
        n12 [label="n12: 12.7%"];
        n13 [label="n13: 53.1%"];
        n14 [label="n14: 29%"];
        n15 [label="n15: 5.1%"];
        n11->n12;
        n12->n13;
        n12->n14;
        n14->n15;
    }

Currently the clusters are not labeled when they are provided to the main reconstruction algorithm, so that the output cannot be labeled either (a future update will remedy this), and they are simply labeled as n (as in node) + their subclone ID. But given the subclone frequencies, it is easy to assign the mutations back to each of the subclones, working from the bottom up.

###Example-4: Analysis of two OvCa cancer sample Single Nucleotide Polymorphism dataset

execute

	make

The details of the resulting structures can be interrogated in a similar fashion as in the previous example
