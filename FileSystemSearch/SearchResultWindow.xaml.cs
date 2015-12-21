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

		public SearchResultWindow(SearchViewModel searchViewModel)
		{
			InitializeComponent();

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

		private void ListViewItem_MouseDoubleClick(object sender, MouseButtonEventArgs e)
		{
			var item = (sender as ListView).SelectedItem as SearchResultViewModel;

            try
            {
                System.Diagnostics.Process.Start(item.Path);
            }
            catch (Exception ex)
            {
                MessageBox.Show(string.Format("Failed to open {0}: {1}.", item.Path, ex.Message), "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
		}

        private void OnProgressUpdated(double progress)
        {
            Dispatcher.InvokeAsync(() =>
            {
                progressBar.IsIndeterminate = false;
                progressBar.Value = progress;
            }, DispatcherPriority.Background);
        }

        private void OnSearchDone()
        {
            Dispatcher.InvokeAsync(() =>
            {
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
	}
}
