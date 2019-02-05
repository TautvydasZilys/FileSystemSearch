using System;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace FileSystemSearch
{
	class SearchUtils
	{
		public delegate void FoundPathCallbackDelegate(IntPtr findData, IntPtr path);
		public delegate void SearchProgressUpdatedDelegate([In] ref SearchStatistics searchStatistics, double progress);
		public delegate void SearchDoneCallbackDelegate([In] ref SearchStatistics searchStatistics);

		public static bool ValidateSearchViewModel(SearchViewModel searchViewModel, out string validationFailedReason)
		{
			if (!searchViewModel.SearchForFiles && !searchViewModel.SearchForDirectories)
			{
				validationFailedReason = "Either search for files, and/or search for directories must be selected.";
				return false;
			}

			if (searchViewModel.SearchForFiles && !searchViewModel.SearchInFilePath && !searchViewModel.SearchInFileName && !searchViewModel.SearchInFileContents)
			{
				validationFailedReason = "At least one file search mode must be selected if searching for directories.";
				return false;
			}

			if (searchViewModel.SearchInFileContents && !searchViewModel.SearchContentsAsUtf8 && !searchViewModel.SearchContentsAsUtf16)
			{
				validationFailedReason = "When searching file contents, either must be searched as either UTF8 and/or UTF16.";
				return false;
			}

			if (searchViewModel.SearchForDirectories && !searchViewModel.SearchInDirectoryPath && !searchViewModel.SearchInDirectoryName)
			{
				validationFailedReason = "At least one directory search mode must be selected if searching for directories.";
				return false;
			}

			if (string.IsNullOrEmpty(searchViewModel.SearchPath))
			{
				validationFailedReason = "Search path must not be empty.";
				return false;
			}

			if (string.IsNullOrEmpty(searchViewModel.SearchPattern))
			{
				validationFailedReason = "Search pattern must not be empty.";
				return false;
			}

			if (string.IsNullOrEmpty(searchViewModel.SearchString))
			{
				validationFailedReason = "Search string must not be empty.";
				return false;
			}

			if (Encoding.UTF8.GetByteCount(searchViewModel.SearchString) > (1 << 30))
			{
				validationFailedReason = "Search string is too long.";
				return false;
			}

			if (searchViewModel.SearchInFileContents && searchViewModel.SearchContentsAsUtf8 && searchViewModel.SearchString.Any(c => c < 0 || c > 127))
			{
				validationFailedReason = "Searching for file contents as UTF8 with non-ascii string is not implemented.";
				return false;
			}

			validationFailedReason = null;
			return true;
		}

		public static IntPtr SearchAsync(SearchViewModel searchViewModel, FoundPathCallbackDelegate foundPathCallback, SearchProgressUpdatedDelegate searchProgressUpdated, SearchDoneCallbackDelegate searchDoneCallback)
		{
			SearchFlags searchFlags = SearchFlags.None;

			if (searchViewModel.SearchForFiles)
				searchFlags |= SearchFlags.SearchForFiles;

			if (searchViewModel.SearchInFileName)
				searchFlags |= SearchFlags.SearchInFileName;

			if (searchViewModel.SearchInFilePath)
				searchFlags |= SearchFlags.SearchInFilePath;

			if (searchViewModel.SearchInFileContents)
				searchFlags |= SearchFlags.SearchInFileContents;

			if (searchViewModel.SearchContentsAsUtf8)
				searchFlags |= SearchFlags.SearchContentsAsUtf8;

			if (searchViewModel.SearchContentsAsUtf16)
				searchFlags |= SearchFlags.SearchContentsAsUtf16;

			if (searchViewModel.SearchForDirectories)
				searchFlags |= SearchFlags.SearchForDirectories;

			if (searchViewModel.SearchInDirectoryPath)
				searchFlags |= SearchFlags.SearchInDirectoryPath;

			if (searchViewModel.SearchInDirectoryName)
				searchFlags |= SearchFlags.SearchInDirectoryName;

			if (searchViewModel.SearchRecursively)
				searchFlags |= SearchFlags.SearchRecursively;

			if (searchViewModel.SearchIgnoreCase)
				searchFlags |= SearchFlags.SearchIgnoreCase;

			if (searchViewModel.SearchIgnoreDotStart)
				searchFlags |= SearchFlags.SearchIgnoreDotStart;

			return Search(foundPathCallback, searchProgressUpdated, searchDoneCallback, searchViewModel.SearchPath, searchViewModel.SearchPattern, searchViewModel.SearchString, searchFlags, searchViewModel.IgnoreFilesLargerThanInBytes);
		}

		enum SearchFlags
		{
			None = 0,
			SearchForFiles = 1 << 0,
			SearchInFileName = 1 << 1,
			SearchInFilePath = 1 << 2,
			SearchInFileContents = 1 << 3,
			SearchContentsAsUtf8 = 1 << 4,
			SearchContentsAsUtf16 = 1 << 5,
			SearchForDirectories = 1 << 6,
			SearchInDirectoryPath = 1 << 7,
			SearchInDirectoryName = 1 << 8,
			SearchRecursively = 1 << 9,
			SearchIgnoreCase = 1 << 10,
			SearchIgnoreDotStart = 1 << 11,
		};

		[DllImport("SearchEngine.dll")]
		private static extern IntPtr Search(
			FoundPathCallbackDelegate foundPathCallback,
			SearchProgressUpdatedDelegate progressUpdatedCallback,
			SearchDoneCallbackDelegate searchDoneCallback,
			[MarshalAs(UnmanagedType.LPWStr)]string searchPath,
			[MarshalAs(UnmanagedType.LPWStr)]string searchPattern,
			[MarshalAs(UnmanagedType.LPWStr)]string searchString,
			SearchFlags searchFlags,
			ulong ignoreFilesLargerThan);

		[DllImport("SearchEngine.dll")]
		public static extern void CleanupSearchOperation(IntPtr searchOperation);
	}

	struct SearchStatistics
	{
		public ulong directoriesEnumerated;
		public ulong filesEnumerated;
		public ulong fileContentsSearched;
		public ulong resultsFound;
		public ulong totalFileSize;
		public ulong scannedFileSize;
		public double searchTimeInSeconds;
	}
}
