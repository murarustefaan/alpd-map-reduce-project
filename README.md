# ALPD.MapReduce
College project for ALPD [Parallel and distributed algorithms] that implements the MapReduce algorithm.

The scope of this project was to implement the MapReduce algorithm using filesystem storage.
Based on some input files, the algorithm was to execute 4 stages of processing, as follows:
- Split the input files into words and write files on a temporary folder with the following signature {word}_{timestamp}

- Build the direct index of the given files by using the previously generated words. The direct index phase will write files in the "direct-index" folder containing the words and their corresponding number of appearances in the original file.

- To avoid data race conditions on writing the appearances of the words(in the initial files) there was implemented another step that creates folders for all words, folders containing the number of appearances of the word in the initial files word/{fileName}_{appearances}_{timestamp}.

- The last step, creating the reverse index, is done after all previous ones are finished. During this phase files are created for all words, containing the initial file name and the corresponding number of appearances.
