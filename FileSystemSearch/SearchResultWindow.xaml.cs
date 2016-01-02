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
	/// <summary>
	/// Interaction logic for SearchResultWindow.xaml
	/// </summary>
	public partial class SearchResultWindow : Window
	{
		private SearchUtils.FoundPathCallbackDelegate foundPathCallback;
		private SearchUtils.SearchProgressUpdatedDelegate searchProgressUpdatedCallback;
		private SearchUtils.SearchDoneCallbackDelegate searchDoneCallback;

		private IntPtr searchOperation;
		private GCHandle windowGCHandle;
		private SearchResultsViewModel resultsViewModel;

		public SearchResultWindow(SearchViewModel searchViewModel)
		{
			InitializeComponent();

			DataContext = resultsViewModel = new SearchResultsViewModel();
			headerStackPanel.DataContext = searchViewModel;

			progressBar.IsEnabled = true;
			progressBar.Value = 0;
			progressBar.IsIndeterminate = true;

			foundPathCallback = resultsView.AddItem;
			searchProgressUpdatedCallback = OnProgressUpdated;
			searchDoneCallback = OnSearchDone;

			searchOperation = SearchUtils.SearchAsync(searchViewModel, foundPathCallback, searchProgressUpdatedCallback, searchDoneCallback);
			windowGCHandle = GCHandle.Alloc(this);
		}

		protected override void OnClosed(EventArgs e)
		{
			CleanupSearchOperationIfNeeded();
			resultsView.Cleanup();
			base.OnClosed(e);
		}

		private void OnProgressUpdated(ref SearchStatistics searchStatistics, double progress)
		{
			var statsSnapshot = searchStatistics;

			Dispatcher.InvokeAsync(() =>
			{
				if (!double.IsNaN(progress))
				{
					progressBar.IsIndeterminate = false;
					progressBar.Value = 100.0 * progress;
				}

				UpdateStats(ref statsSnapshot);
			}, DispatcherPriority.Background);
		}

		private void OnSearchDone(ref SearchStatistics searchStatistics)
		{
			var statsSnapshot = searchStatistics;

			Dispatcher.InvokeAsync(() =>
			{
				UpdateStats(ref statsSnapshot);
                progressBar.IsIndeterminate = false;
				progressBar.IsEnabled = false;
				progressBar.Value = 100;
				CleanupSearchOperationIfNeeded();
				windowGCHandle.Free();
			}, DispatcherPriority.Background);
		}

		private void CleanupSearchOperationIfNeeded()
		{
			if (searchOperation != IntPtr.Zero)
			{
				var operation = searchOperation;
				searchOperation = IntPtr.Zero;

				Task.Run(() =>
				{
					SearchUtils.CleanupSearchOperation(operation);
				});
			}
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
