using System;
using System.Collections.ObjectModel;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;

namespace FileSystemSearch
{
	public partial class SearchResultWindow : Window
	{
		private SearchUtils.FoundPathCallbackDelegate foundPathCallback;
		private SearchUtils.SearchProgressUpdatedDelegate searchProgressUpdatedCallback;
		private SearchUtils.SearchDoneCallbackDelegate searchDoneCallback;
		private SearchUtils.ErrorCallbackDelegate errorCallback;

		private IntPtr searchOperation;
		private GCHandle windowGCHandle;
		private SearchResultsViewModel resultsViewModel;
		private AtomicBox<SearchStatistics> latestSearchStatistics;
		private bool isClosing;
		private volatile bool isProgressUpdating;

		public SearchResultWindow(SearchViewModel searchViewModel)
		{
			InitializeComponent();

			Title = string.Format("Results for \"{0}\" - FileSystemSearch", searchViewModel.SearchString);
			DataContext = resultsViewModel = new SearchResultsViewModel();
			headerStackPanel.DataContext = searchViewModel;

			progressBar.IsEnabled = true;
			progressBar.Value = 0;
			progressBar.IsIndeterminate = true;

			foundPathCallback = resultsView.AddItem;
			searchProgressUpdatedCallback = OnProgressUpdated;
			searchDoneCallback = OnSearchDone;
			errorCallback = OnError;

			{
				var defaultSearchStatistics = default(SearchStatistics);
				latestSearchStatistics.Set(ref defaultSearchStatistics);
			}

			searchOperation = SearchUtils.SearchAsync(searchViewModel, foundPathCallback, searchProgressUpdatedCallback, searchDoneCallback, errorCallback);
			if (searchOperation == IntPtr.Zero)
				return;

			windowGCHandle = GCHandle.Alloc(this);
		}

        protected override async void OnClosed(EventArgs e)
		{
			isClosing = true;
			base.OnClosed(e);

			await CleanupSearchOperationIfNeeded();
			resultsView.Cleanup();
		}

		private void OnProgressUpdated(ref SearchStatistics searchStatistics, double progress)
		{
			latestSearchStatistics.Set(ref searchStatistics);

			// Don't queue up new progress updates if progress is already updating
			// This prevents overloading WPF dispatcher queue, making UI more responsive
			if (isProgressUpdating)
				return;

			isProgressUpdating = true;

			Dispatcher.InvokeAsync(() =>
			{
				if (!double.IsNaN(progress))
				{
					progressBar.IsIndeterminate = false;
					progressBar.Value = 100.0 * progress;
				}

				var statsSnapshot = latestSearchStatistics.Get();
				UpdateStats(ref statsSnapshot);

				isProgressUpdating = false;
			}, DispatcherPriority.Input);
		}

		private void OnSearchDone(ref SearchStatistics searchStatistics)
		{
			latestSearchStatistics.Set(ref searchStatistics);

            Dispatcher.InvokeAsync(async () =>
            {
                await FinishSearch();
            }, DispatcherPriority.Input);
		}

        private void OnError(string message)
		{
			Dispatcher.InvokeAsync(async () =>
			{
				MessageBox.Show(message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
				await FinishSearch();
			}, DispatcherPriority.Input);
		}

		private async Task FinishSearch()
		{
			if (!isClosing)
			{
				var statsSnapshot = latestSearchStatistics.Get();
				UpdateStats(ref statsSnapshot);
				progressBar.IsIndeterminate = false;
				progressBar.IsEnabled = false;
				progressBar.Value = 100;
			}

			await CleanupSearchOperationIfNeeded();

			if (windowGCHandle.IsAllocated)
				windowGCHandle.Free();
		}

		private async Task CleanupSearchOperationIfNeeded()
		{
			if (searchOperation == IntPtr.Zero)
				return;

			var operation = searchOperation;
			searchOperation = IntPtr.Zero;

			await Task.Run(() =>
			{
				SearchUtils.CleanupSearchOperation(operation);
			});
		}

		private void UpdateStats(ref SearchStatistics searchStatistics)
		{
			resultsViewModel.DirectoriesEnumerated = searchStatistics.directoriesEnumerated;
			resultsViewModel.FilesEnumerated = searchStatistics.filesEnumerated;
			resultsViewModel.FileContentsSearched = searchStatistics.fileContentsSearched;
			resultsViewModel.ResultsFound = searchStatistics.resultsFound;
			resultsViewModel.TotalEnumeratedFilesSizeInBytes = searchStatistics.totalFileSize;
			resultsViewModel.TotalContentSearchedFilesSizeInBytes = searchStatistics.scannedFileSize;
			resultsViewModel.SearchTimeInSeconds = searchStatistics.searchTimeInSeconds;
		}
	}
}
